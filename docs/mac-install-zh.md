# macOS 编译安装指南

本文档详细说明如何在 macOS 上编译和安装 Gemini Watermark Tool。

## 系统要求

- macOS 11.0 或更高版本
- Apple Silicon (arm64) 或 Intel (x86_64) 处理器
- Homebrew 包管理器
- Xcode Command Line Tools

## 安装步骤

### 1. 安装 Homebrew（如果尚未安装）

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. 安装依赖

使用 Homebrew 安装所有必需的依赖项：

```bash
brew install ninja opencv fmt cli11 spdlog
```

**依赖说明：**
- `ninja` - 快速构建工具
- `opencv` - 图像处理库
- `fmt` - 格式化库
- `cli11` - 命令行参数解析库
- `spdlog` - 日志库

### 3. 克隆项目（如果尚未克隆）

```bash
git clone https://github.com/allenk/GeminiWatermarkTool.git
cd GeminiWatermarkTool
```

### 4. 配置构建

根据你的需求选择以下方案之一：

#### 方案 A：构建 arm64 版本（仅 Apple Silicon）

适合只在本机运行的情况：

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
```

#### 方案 B：构建 x86_64 版本（仅 Intel）

**注意：** 在 Apple Silicon Mac 上，需要使用 Rosetta 环境下的 Homebrew 和依赖。

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
```

#### 方案 C：构建通用二进制（需要 vcpkg）

如果需要同时支持 arm64 和 x86_64，建议使用 vcpkg 而不是 Homebrew。参考项目的 `vcpkg.json` 配置。

### 5. 编译项目

```bash
cmake --build build
```

编译成功后，可执行文件位于 `build/GeminiWatermarkTool`。

### 6. 验证构建

检查二进制文件信息：

```bash
# 查看文件类型
file build/GeminiWatermarkTool

# 查看架构信息
lipo -info build/GeminiWatermarkTool

# 测试运行
./build/GeminiWatermarkTool --version
```

### 7. 复制到项目根目录（可选）

```bash
cp build/GeminiWatermarkTool .
```

### 8. 配置命令行别名（可选）

为了方便使用，可以创建一个 shell 别名：

#### 方法 1：使用独立的 alias 文件（推荐）

1. 创建 `~/.alias` 文件：

```bash
cat > ~/.alias << 'EOF'
# Personal aliases

# Gemini Watermark Tool
alias gwt='/Users/你的用户名/GeminiWatermarkTool/GeminiWatermarkTool'
EOF
```

2. 在 `~/.zshrc` 中添加加载配置：

```bash
echo '
# Load personal aliases
[ -f ~/.alias ] && source ~/.alias' >> ~/.zshrc
```

3. 重新加载配置：

```bash
source ~/.zshrc
```

#### 方法 2：直接添加到 .zshrc

```bash
echo "alias gwt='$(pwd)/GeminiWatermarkTool'" >> ~/.zshrc
source ~/.zshrc
```

### 9. 使用

设置别名后，可以在任何目录直接使用：

```bash
gwt --help
gwt --version
gwt add input.jpg -o output.jpg
gwt remove watermarked.jpg -o restored.jpg
```

## 常见问题

### Q: 编译时提示找不到依赖库

**A:** 确保所有依赖都已通过 Homebrew 安装：

```bash
brew list ninja opencv fmt cli11 spdlog
```

如果某个包未安装，运行：

```bash
brew install <包名>
```

### Q: 在 Apple Silicon Mac 上无法构建 x86_64 版本

**A:** Homebrew 在 Apple Silicon 上默认安装 arm64 版本的库。要构建 x86_64 版本，需要：

1. 安装 Rosetta 2：`softwareupdate --install-rosetta`
2. 在 Rosetta 环境下安装第二个 Homebrew（位于 `/usr/local`）
3. 使用该 Homebrew 安装 x86_64 版本的依赖

或者使用 vcpkg 进行跨架构编译。

### Q: 构建通用二进制（Universal Binary）

**A:** 使用 Homebrew 无法直接构建通用二进制，因为 Homebrew 的库是单架构的。建议使用 vcpkg：

```bash
# 安装 vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# 使用 CMake Preset 构建
cmake --preset mac-universal-Release
cmake --build --preset mac-universal-Release
```

### Q: 运行时提示找不到动态库

**A:** 确保 Homebrew 的库路径在系统路径中：

```bash
export DYLD_LIBRARY_PATH=/opt/homebrew/lib:$DYLD_LIBRARY_PATH
```

或者重新编译时静态链接。

## 清理构建

如果需要重新构建，可以清理构建目录：

```bash
rm -rf build
```

## 更多信息

- 项目主页：https://github.com/allenk/GeminiWatermarkTool
- 问题反馈：https://github.com/allenk/GeminiWatermarkTool/issues
