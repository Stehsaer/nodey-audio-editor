# Nodey Audio Editor

> 中山大学计算机学院2024学年春软件工程大作业项目
> 
> 遵循GPL-2.0-or-later开源协议

Nodey Audio Editor是一个使用C++编写的，基于可视化节点图的音频效果软件。

## 主要功能

- 多格式的音频导入
- 导出MP3格式
- 实时预览/播放处理结果
- 多个内置音频处理节点
  - 输入/输出
  - 声道分离与混合
  - 音频混合
  - 音调与速度调节

## 依赖的库

- [ImGui](https://github.com/ocornut/imgui)，易用的Immediate Mode GUI库，用于构建基础用户界面（MIT）
- [SDL2](https://www.libsdl.org/)，跨平台的多媒体库，用于跨平台显示窗口与播放音频（zlib）
- [ImNodes](https://github.com/Nelarius/imnodes)，功能较为完善的节点渲染系统（MIT）
- [FFmpeg](https://ffmpeg.org/)，著名的音频编解码库，项目使用了其中的`libavcodec`和`libswresample`库（GPL-3.0）
- [Lame](https://lame.sourceforge.io/)，MP3编码库，用来实现MP3导出功能（LGPL-2.0-or-later）
- [JsonCPP](https://github.com/open-source-parsers/jsoncpp)，轻量级JSON编解码库，用于保存和加载项目文件（MIT）
- [FFTW](https://fftw.org/)，简单的快速傅里叶变换算法库，用于实现频谱显示（GPL-2.0）
- [Boost.Fiber](https://www.boost.org/doc/libs/1_81_0/libs/fiber/doc/html/fiber/overview.html)，Boost纤程库，用于实现多个处理器的并发运行（BSL-1.0）
- [SoundTouch](https://www.surina.net/soundtouch/index.html)，音频变调/变速库，用于实现变调/变速节点算法（LGPL-2.1）
- [Portable File Dialogs](https://github.com/samhocevar/portable-file-dialogs)，跨平台文件保存/打开窗口库，用于保存/加载项目文件、导入/导出音频文件（WTFPL）