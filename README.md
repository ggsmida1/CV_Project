# 字符残缺检测系统 (Character Defect Detection)

计算机视觉课程设计项目

## 项目简介

基于 Qt 和 OpenCV 开发的字符残缺检测系统，用于检测印刷字符、标识等是否存在残缺、磨损等问题。系统采用模板匹配定位 + 区域差异对比的方法，实现对多个检测区域的自动化质量检测。

## 功能特性

### 1. 模板设计模块

- 加载标准模板图片
- 鼠标拖拽选择检测区域（ROI）
- 支持多个检测区域的添加、删除、命名
- 配置文件保存/加载（JSON格式，包含模板图像和ROI信息）

### 2. 检测模块

- 单张图片检测
- 批量图片检测
- 自动倾斜矫正（霍夫变换检测直线角度）
- 模板匹配定位（归一化相关系数法）
- 字符残缺检测（二值化差异对比）
- 实时显示检测结果（绿色=正常，红色=残缺）

### 3. 结果管理模块

- 检测结果表格展示
- CSV格式导出
- 结果清空

## 环境要求

| 组件     | 版本要求    |
| ------ | ------- |
| Qt     | 5.14.2+ |
| OpenCV | 4.10.0+ |
| MinGW  | 64-bit  |
| C++    | C++11   |

## 编译说明

### 1. 安装依赖

- 安装 Qt (MinGW 版本，推荐 5.14.2 或更高)
- 安装 OpenCV (MinGW 编译版本，推荐 4.10.0 或更高)

### 2. 配置 OpenCV 路径

设置环境变量 `OPENCV_DIR` 指向 OpenCV 安装目录，结构需包含 `include/` 和 `x64/mingw/lib/` 子目录：

```
OPENCV_DIR = D:/your/opencv/install/path
```

或在 `opencv-last/opencv-last.pro` 中修改默认路径。

### 3. 编译步骤（两种方式二选一）

#### 方式 A：一键脚本（推荐，无需 Qt Creator）

```powershell
cd opencv-last
# 打开 build.ps1，修改顶部 CONFIG 区的 5 个路径为你本地 Qt / MinGW / OpenCV 路径
.\build.ps1
```

脚本会自动：
1. 调用 `qmake` 生成 Makefile（中间产物在 `build/`）
2. 调用 `mingw32-make` 编译
3. 把 Qt / OpenCV 运行所需 DLL 和 `platforms/` 插件复制到 `bin/`
4. 启动程序

编译后目录结构：
```
opencv-last/
├── build/                 # 编译中间产物（不提交）
└── bin/                   # 可执行程序 + 依赖 DLL（不提交）
    ├── CharacterDefectDetection.exe
    ├── Qt5Core.dll / Qt5Gui.dll / Qt5Widgets.dll
    ├── libopencv_world4100.dll
    └── platforms/qwindows.dll
```

#### 方式 B：使用 Qt Creator

1. Qt Creator 打开 `opencv-last/opencv-last.pro`
2. 执行 qmake
3. 构建项目

### 4. 团队协作注意事项

- `build/` 和 `bin/` 不会提交到 Git（已在 `opencv-last/.gitignore`）
- `build.ps1` **会提交到仓库**作为模板。每位开发者首次使用时在本地编辑 `CONFIG` 区的 5 个路径即可（不要把自己的本地路径改动提交回共享仓库，除非是调整默认示例）
- 如添加新的源文件，请在 `opencv-last.pro` 的 `SOURCES` / `HEADERS` 里同步更新

## 使用方法

### 模板设计流程

1. 点击「加载模板」选择标准模板图片
2. 点击「添加区域」后在图片上拖拽鼠标选择检测区域
3. 重复步骤2添加多个检测区域
4. 点击「保存配置」保存为 JSON 文件

### 检测流程

1. 点击「加载配置」导入之前保存的配置文件
2. 点击「加载待测」选择待检测图片，或点击「批量检测」选择多张图片
3. 点击「开始检测」执行检测
4. 查看检测结果表格和标记图像
5. 点击「导出结果」保存为 CSV 文件

## 技术原理

### 倾斜矫正

- 灰度化 + OTSU二值化
- 霍夫变换检测直线
- 计算平均倾斜角度
- 仿射变换旋转矫正

### 模板匹配定位

- 使用 `cv::matchTemplate` 进行归一化相关系数匹配
- 找到模板在待测图像中的最佳匹配位置

### 残缺检测

- 提取检测区域图像
- 高斯模糊去噪
- OTSU二值化
- 计算与模板区域的绝对差异
- 形态学开运算去除噪声
- 差异比例超过阈值（5%）判定为残缺

## 项目结构

```
opencv-last/
├── main.cpp              # 程序入口
├── mainwindow.h          # 主窗口头文件
├── mainwindow.cpp        # 主窗口实现
├── mainwindow.ui         # UI界面文件
├── resources.qrc         # Qt 资源文件（含样式）
├── style.qss             # 界面样式（深色工业风）
├── opencv-last.pro       # Qt项目配置文件
├── build.ps1             # 一键构建脚本（首次使用需编辑路径）
├── .gitignore            # Git忽略配置
├── build/                # 编译中间产物（不提交）
└── bin/                  # 可执行程序 + 依赖DLL（不提交）
```

## 作者

黄浩城，莫顺彬

## 许可证

本项目仅供学习和研究使用。
