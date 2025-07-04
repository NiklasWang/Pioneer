#include <wayland-client.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "xdg-shell-client-protocol.h"

// 客户端状态结构体
struct client_state {
    // Wayland 核心对象
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *xdg_wm_base;
    
    // 窗口相关对象
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    
    // 缓冲区
    struct wl_buffer *buffer;
    
    // 窗口属性
    int width;
    int height;
    uint32_t color; // ARGB格式颜色
};

// 前向声明函数
static void draw_frame(struct client_state *state);
static struct wl_buffer *create_shm_buffer(struct client_state *state);
static void registry_global(void *data, struct wl_registry *registry,
                           uint32_t id, const char *interface, uint32_t version);
static void xdg_surface_configure(void *data, 
                                 struct xdg_surface *xdg_surface,
                                 uint32_t serial);
static void xdg_toplevel_configure(void *data, 
                                  struct xdg_toplevel *xdg_toplevel,
                                  int32_t width, int32_t height,
                                  struct wl_array *states);
static void xdg_toplevel_close(void *data, 
                              struct xdg_toplevel *xdg_toplevel);

// 注册表监听器
static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = NULL,
};

// XDG Surface 监听器
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// XDG Toplevel 监听器
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

// 全局注册表监听器回调
static void registry_global(void *data, struct wl_registry *registry,
                           uint32_t id, const char *interface, uint32_t version) {
    struct client_state *state = data;
    printf("Got a registry event for %s id %d\n", interface, id);    
    if (strcmp(interface, "wl_compositor") == 0) {
        state->compositor = wl_registry_bind(
            registry, id, &wl_compositor_interface, 4);
    } else if (strcmp(interface, "wl_shm") == 0) {
        state->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        state->xdg_wm_base = wl_registry_bind(
            registry, id, &xdg_wm_base_interface, 1);
    }
}

// XDG Surface 配置回调
static void xdg_surface_configure(void *data, 
                                 struct xdg_surface *xdg_surface,
                                 uint32_t serial) {
    struct client_state *state = data;
    
    // 必须确认配置事件
    xdg_surface_ack_configure(xdg_surface, serial);
    
    // 调用绘制函数
    draw_frame(state);
}

// XDG Toplevel 配置回调
static void xdg_toplevel_configure(void *data, 
                                  struct xdg_toplevel *xdg_toplevel,
                                  int32_t width, int32_t height,
                                  struct wl_array *states) {
    struct client_state *state = data;
    
    // 更新窗口大小（如果有效）
    if (width > 0 && height > 0) {
        state->width = width;
        state->height = height;
        printf("窗口大小调整为: %dx%d\n", width, height);
    }
}

// XDG Toplevel 关闭回调
static void xdg_toplevel_close(void *data, 
                              struct xdg_toplevel *xdg_toplevel) {
    struct client_state *state = data;
    printf("窗口关闭请求\n");
    
    // 设置标志或直接退出
    wl_display_cancel_read(state->display);
    wl_display_disconnect(state->display);
    exit(0);
}

// 创建共享内存缓冲区
static struct wl_buffer *create_shm_buffer(struct client_state *state) {
    int stride = state->width * 4; // 每行字节数 (ARGB8888)
    int size = stride * state->height;
    
    // 创建临时文件
    char filename[] = "/tmp/wayland-shm-XXXXXX";
    int fd = mkstemp(filename);
    if (fd < 0) {
        fprintf(stderr, "创建临时文件失败: %s\n", strerror(errno));
        return NULL;
    }
    
    // 设置文件大小
    if (ftruncate(fd, size) < 0) {
        fprintf(stderr, "设置文件大小失败: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    
    // 内存映射
    uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "内存映射失败: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    
    // 用指定颜色填充
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            data[y * state->width + x] = state->color;
        }
    }
    
    // 创建 Wayland 缓冲区
    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, state->width, state->height, stride, WL_SHM_FORMAT_ARGB8888);
    
    // 清理资源
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
    unlink(filename);
    
    return buffer;
}

// 绘制窗口内容
static void draw_frame(struct client_state *state) {
    // 创建新缓冲区
    struct wl_buffer *buffer = create_shm_buffer(state);
    if (!buffer) {
        fprintf(stderr, "创建缓冲区失败\n");
        return;
    }
    
    // 销毁旧缓冲区
    if (state->buffer) {
        wl_buffer_destroy(state->buffer);
    }
    state->buffer = buffer;
    
    // 设置表面内容
    wl_surface_attach(state->surface, state->buffer, 0, 0);
    wl_surface_damage(state->surface, 0, 0, state->width, state->height);
    wl_surface_commit(state->surface);
}

int main(int argc, char *argv[]) {
    struct client_state state = {0};
    
    // 1. 连接到Wayland服务器
    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "无法连接到Wayland显示服务器\n");
        return EXIT_FAILURE;
    }
    
    // 2. 获取注册表
    state.registry = wl_display_get_registry(state.display);
    
    // 3. 添加注册表监听器
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    
    // 4. 同步获取全局对象
    wl_display_roundtrip(state.display);
    
    // 检查必要的全局对象
    if (!state.compositor || !state.shm || !state.xdg_wm_base) {
        fprintf(stderr, "缺少必要的全局对象\n");
        return EXIT_FAILURE;
    }
    
    // 5. 设置窗口属性
    state.width = 800;
    state.height = 600;
    state.color = 0x0A0000FF; // ARGB格式: 不透明蓝色
    
    // 6. 创建表面
    state.surface = wl_compositor_create_surface(state.compositor);
    if (!state.surface) {
        fprintf(stderr, "无法创建表面\n");
        return EXIT_FAILURE;
    }
    
    // 7. 创建 XDG Surface
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
        state.xdg_wm_base, state.surface);
    if (!state.xdg_surface) {
        fprintf(stderr, "无法创建XDG Surface\n");
        return EXIT_FAILURE;
    }
    
    // 8. 添加 XDG Surface 监听器
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    
    // 9. 创建顶层窗口
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    if (!state.xdg_toplevel) {
        fprintf(stderr, "无法创建XDG Toplevel\n");
        return EXIT_FAILURE;
    }
    
    // 10. 添加 XDG Toplevel 监听器
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);
    
    // 11. 设置窗口标题
    xdg_toplevel_set_title(state.xdg_toplevel, "Wayland 客户端示例");
    
    // 12. 提交初始配置
    wl_surface_commit(state.surface);
    
    // 13. 等待初始配置事件
    wl_display_roundtrip(state.display);
    
    // 14. 绘制初始帧
    draw_frame(&state);
    
    // 15. 事件循环
    while (wl_display_dispatch(state.display) != -1) {
        // 主循环 - 可以在这里处理输入或动画
    }
    
    // 16. 清理资源
    if (state.buffer) wl_buffer_destroy(state.buffer);
    if (state.xdg_toplevel) xdg_toplevel_destroy(state.xdg_toplevel);
    if (state.xdg_surface) xdg_surface_destroy(state.xdg_surface);
    if (state.surface) wl_surface_destroy(state.surface);
    if (state.xdg_wm_base) xdg_wm_base_destroy(state.xdg_wm_base);
    if (state.shm) wl_shm_destroy(state.shm);
    if (state.compositor) wl_compositor_destroy(state.compositor);
    wl_registry_destroy(state.registry);
    wl_display_disconnect(state.display);
    
    return EXIT_SUCCESS;
}
