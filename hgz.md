


一、LVGL原架构概览
1、使用LVGL的典型应用的技术架构
[图片]
说明：
（1）LVGL应用代码，叫做Application Space应用空间，入口是main.c；
（2）LVGL Core 实体空间，上承接应用的调用，下推送显示数据，读取触控输入，等等。
（3）硬件驱动空间，关键的有：显示flush函数处理，触控读入处理。下接LCD显示控制器与触控传感器。
2、交互流图（从输入到用户界面更新）
[图片]
说明：
（1）LVGL的软件架构，依赖于硬件触控或定时器，硬件触控或定时器循环触发LVGL Core层函数lv_timer_handler()；
（2）lv_timer_handler()函数调用touchpad_read()函数，获取触控事件LV_INDEV_STATE_PRESSED，并转为LV_EVENT_CLICKED，发送给目标对像lv_obj_XXXX_t；
（3）目标对象执行callback函数，比如：btn_event_cb，并且调用LVGL Core层函数Invalidate更新脏区域(lv_obj_invalidate)——这一步，会触发渲染，修改脏区域存储在lv_disp_buffer_t的像素；
（4）LVGL Core层timerhandler在渲染完毕后，调用显示模块lv_display_t的disp_flush()函数，把存储在lv_disp_buffer_t的像素推送到硬件LCD。推送完毕后，再次回调LVGL Core层的lv_display_flush_ready()回调函数，准备开始下一帧处理。

二、核心架构展开说明
1、核心数据结构（包括：一些如何用C语言，实现类似C++的面向对象的逻辑）
LVGL 采用分层对象模型设计，其中控件（对象）在特定于显示器的树状结构中进行管理。运行时环境由集中式全局状态和基于定时器的事件循环驱动，该循环同步输入处理、布局重新计算和显示刷新。
[图片]
2、对象模型（lv_obj）与组件Widget创建流程
LVGL 中每个控件的基础都是lv_obj_t结构。详见src/core/lv_obj_private.h第42-100行。
对象以父子层次结构组织，子对象的位置相对于父对象，并且当父对象销毁时，子对象也会自动删除。
该系统使用由以下方式定义的基于类的继承模型：lv_obj_class_t（src/core/lv_obj.c第209-221）
允许组件重写构造函数、析构函数和事件处理程序。
[图片]

三、图形渲染相关架构——显示管理
显示和输入子系统构成了 LVGL 的硬件抽象层。它们将高级 UI 逻辑（控件、布局、事件）与低级硬件需求（像素推送、触摸中断、按钮扫描）分离。
系统通过定时器驱动的流水线运行：
1. 输入读取：lv_indev_read定期调用以轮询硬件状态（src/indev/lv_indev.c第219-223行）
2. 状态处理：输入数据被映射到对象（命中测试），并触发诸如点击或滚动之类的事件。（src/indev/lv_indev.c第63-66行）
3. 显示刷新：refr_timer识别“脏”（失效）区域，并调用绘图引擎更新屏幕。（src/display/lv_display.c第115-120）
[图片]
上图，仅仅表达了显示的对象lv_display_t，以及输入对象lv_indev_t，对于图形渲染，重点关注lv_display_t。
下面重点展开：
1、显示管理：
LVGL 中的显示管理以lv_display_t对象为中心，对象封装了物理或虚拟屏幕的分辨率、缓冲策略和渲染管线。它管理屏幕的生命周期，为全局 UI 元素提供专用层，并通过失效和区域合并机制优化渲染。
lv_display_t 生命周期
该lv_display_t结构是所有输出设备的根实体。它是使用lv_display_create()全局链表创建和管理的。 src/display/lv_display.c22
创建和初始化
当通过以下方式创建显示时lv_display_create(hor_res, ver_res)，系统：
1. 分配lv_display_t结构体并将其添加到全局列表中src/display/lv_display.c68-70
2. 初始化默认参数，例如 DPI、颜色格式和抗锯齿。src/display/lv_display.c74-82
3. 创建刷新计时器（refr_timer），该计时器lv_display_refr_timer按由定义的周期触发。LV_DEF_REFR_PERIOD src/display/lv_display.c115
4. 实例化基础层级结构：bottom_layer，act_scr（默认屏幕），top_layer和sys_layer src/display/lv_display.c146-149
渲染模式和绘制缓冲区
LVGL 支持三种主要渲染模式，可通过配置lv_display_set_render_mode()。这些模式决定了lv_draw_buf_t刷新周期内缓冲区的使用方式。
暂时无法在飞书文档外展示此内容
缓冲区管理
一个显示器最多可以有三个缓冲区（buf_1，buf_2，buf_3）src/display/lv_display_private.h65-67这flush_cb是必需的回调函数，它将渲染后的图像从内部缓冲区传输到硬件。src/display/lv_display_private.h74
失效和区域合并
为了优化性能，LVGL 只重绘屏幕上已更改的部分。这是通过“失效”机制实现的。
刷新管道
1. 失效（或者被标脏）：当对象发生变化时，lv_obj_invalidate()将其面积添加到inv_areasin 中。lv_display_t src/display/lv_display_private.h126
2. 区域合并：在渲染之前，lv_refr_join_area()合并重叠或相邻的无效区域，以减少绘制调用次数。src/core/lv_refr.c36
3. 刷新定时器：lv_display_refr_timer定期运行。它会调用refr_invalid_areas()遍历最终合并区域的函数。src/core/lv_refr.c37
数据流图：刷新逻辑
下图描绘了从失效请求到最终硬件刷新的路径。
[图片]
2、绘图引擎
LVGL绘图引擎采用多层架构，旨在抽象化各种渲染后端的复杂性——从简单的软件像素操作到高级的硬件GPU加速。它利用基于任务的管线，将高级控件请求分解为离散的绘制任务，由可用的硬件或软件单元进行评估，并按优化的顺序执行。
架构概述
渲染管线以三个主要实体为中心：绘制任务、绘制单元和图层。
系统组件图
下图说明了高级绘图概念如何映射到引擎中的特定代码实体。
暂时无法在飞书文档外展示此内容
绘图引擎实体关系
[图片]
绘制任务流程
lv_draw_rect管道管理绘制操作的生命周期。当一个控件需要渲染时，它会调用类似 ` draw` 或 `drawView` 这样的高级函数lv_draw_image。这些函数不会立即绘制；相反，它们会调用 `addView`lv_draw_add_task将新的控件添加lv_draw_task_t到当前的控件中lv_layer_t。
关键阶段包括：
1. 任务创建：lv_draw_add_task分配并初始化任务src/draw/lv_draw.c96-128
2. 评估：每个注册用户lv_draw_unit_t通过评估任务evaluate_cb来提供一个“偏好分数”。src/draw/lv_draw.c150-163
3. 调度：lv_draw_dispatch将任务分配给得分最高的单元，并处理任务之间的依赖关系。src/draw/lv_draw.c210-228
绘制缓冲区管理
lv_draw_buf_t是用于表示任何绘图发生的内存区域的结构。它抽象了特定于硬件的要求，例如：
- 对齐和步长：通过以下方式确保行与特定的字节边界对齐（例如，为了提高 DMA 或 GPU 效率）lv_draw_buf_width_to_stride src/draw/lv_draw_buf.c98-108
- 颜色格式：处理从LV_COLOR_FORMAT_RGB565到各种格式LV_COLOR_FORMAT_ARGB8888 src/draw/lv_draw_buf.c215-217
- 缓存完整性：为具有非一致性内存的系统提供flush_cache_cb接口invalidate_cache_cbsrc/draw/lv_draw_buf.c121-163
矢量图形与3D渲染
除了标准的 UI 基本元素之外，该引擎还支持高级矢量和 3D 功能：
- 矢量 API：lv_draw_vector提供基于路径的 API，用于复杂形状和类似 SVG 的渲染，通常与 ThorVG 集成。src/draw/sw/lv_draw_sw.c101-108
- 3D 视口：专用任务（LV_DRAW_TASK_TYPE_3D）允许在 2D UI 层级结构中渲染 3D 网格和纹理。src/draw/lv_draw_3d.c
3D渲染子系统
LVGL 的 3D 子系统允许将 3D 场景集成到 2D 用户界面中。这是通过lv_3dviewport控件和 GPU 加速实现的lv_draw_gpu_renderer。

3D 视口 (lv_3dviewport)
该lv_3dviewport组件用作 3D 场景的容器。它管理摄像机、灯光和 3D 对象（网格）列表。
- 相机控制：支持偏航、俯仰和视场角（FOV）调整src/libs/gltf/gltf_view/lv_gltf_view.cpp204-248
- 渲染循环：当控件失效时，会触发一个 3D 渲染任务。输出通常渲染到纹理上，然后将其混合到 2D 用户界面中。src/libs/gltf/gltf_view/lv_gltf_view_render.cpp135-161
GPU渲染器帧图
对于复杂的 3D 场景，LVGL 在 GPU 渲染器中采用了基于帧图的方法。这通过管理不同渲染通道（例如，阴影贴图、不透明通道、透明通道）之间的依赖关系来优化渲染管线。
- OpenGL ES 集成：该系统利用 OpenGL ES 实现高性能渲染。src/draw/opengles/lv_draw_opengles.c92-109
- 纹理缓存：渲染的3D对象通常会被缓存为纹理，以最大限度地减少UI切换期间的GPU负载。src/draw/opengles/lv_draw_opengles.c38-48
3D渲染代码实体映射
标题：3D渲染管线实体
资料来源：src/libs/gltf/gltf_view/lv_gltf_view_internal.h39-66 src/draw/opengles/lv_draw_opengles.c35-41 src/drivers/opengles/lv_opengles_driver.c87-96
关键功能和数据结构
暂时无法在飞书文档外展示此内容
glTF 模型加载
LVGL 使用该fastgltf库来解析 glTF 2.0 模型src/libs/gltf/gltf_view/lv_gltf_view.cpp16-26装载机处理：
- 网格数据：顶点缓冲区、索引缓冲区和图元属性src/libs/gltf/gltf_view/lv_gltf_view_render.cpp92-94
- 材质：PBR（基于物理的渲染）属性，包括基础颜色、金属粗糙度和法线贴图src/libs/gltf/gltf_view/lv_gltf_view_render.cpp100-103
- 动画：骨骼动画和蒙皮src/libs/gltf/gltf_view/lv_gltf_view_render.cpp79




