# cpp-webserver — Linux 高并发 HTTP 服务器

基于 **epoll 边缘触发（ET）+ 非阻塞 IO + Reactor 多线程** 模型实现的轻量级 C++ HTTP 服务器，
支持静态资源服务、文件上传/下载服务器、keep-alive 连接管理与空闲连接超时踢除、双缓冲异步日志。

> **来源说明**：本项目是跟随开源教程 [Wlgls/30daysCppWebServer](https://github.com/Wlgls/30daysCppWebServer)
> 逐日学习后的实践成果，代码以教程 day29 完成版为基准整理；在本机新编译环境下修复了两处兼容性问题，
> 压测数据全部为本机实测（见下文）。

## 功能特性

- Reactor 网络模型：main loop + 多 IO loop（one loop per thread），`EventLoopThreadPool` 按线程分配事件循环
- epoll **边缘触发（ET）** + 非阻塞 IO，`Channel` / `Epoller` 封装事件注册与分发
- 应用层读写缓冲 `Buffer`（读写下标分离，可写空间自动扩容）
- HTTP/1.1 请求解析状态机（GET / POST；请求行、头部、body）
- keep-alive 连接复用 + timerfd 定时器管理空闲连接（超时主动踢除）
- 静态文件服务（`sendfile` 零拷贝下载）、文件上传、文件服务器（列表 / 删除 / 下载）
- 日志系统：同步输出与双缓冲异步 `AsyncLogging` 两套（demo 默认同步输出到 stdout）
- 构建：CMake，C++14，`-g -Wall -Werror`

## 目录结构

| 模块 | 路径 | 内容 |
|---|---|---|
| base | `base/` | 通用宏、当前线程信息、CountDownLatch |
| tcp | `tcp/` | Acceptor、Buffer、Channel、Epoller、EventLoop(Thread/ThreadPool)、TcpConnection、TcpServer |
| http | `http/` | HttpContext（解析状态机）、HttpRequest、HttpResponse、HttpServer |
| timer | `timer/` | TimeStamp、Timer、TimerQueue（timerfd 实现） |
| log | `log/` | Logging、LogStream、LogFile、AsyncLogging |
| test | `test/` | 服务器主程序 `http_server.cpp` 及模块单测 |
| static / files | `static/`、`files/` | 演示页面与上传文件目录 |

## 构建

```bash
# 依赖：g++（C++14）、cmake >= 3.10、libjsoncpp-dev、pthread
sudo apt install build-essential cmake libjsoncpp-dev

cmake -S . -B build
cmake --build build -j
```

> 在 g++ 13 等较新编译器下，教程原始代码有两处需要修复（本仓库提交历史中可直接看到）：
> 1. `std::move` 作用于临时对象，触发 `-Werror=pessimizing-move`（`tcp/Buffer.cpp`、`log/LogFile.cpp`）；
> 2. `timer/TimeStamp.h` 依赖了旧编译器传递包含的 `<time.h>`，导致 `struct tm` / `localtime_r` 找不到。

## 运行

```bash
cd build
./test/webserver 1234     # 端口可自定义，默认 1234
```

程序以相对路径读取 `../static` 与 `../files`，**请在 `build/` 目录下运行**。
浏览器访问 `http://127.0.0.1:1234/`（首页）、`http://127.0.0.1:1234/fileserver`（文件服务器）。

## 压测（webbench，本机实测）

测试环境：

- CPU：Intel Core i9-14900HX（WSL2 可见 32 逻辑核），内存配额 7.6 GiB
- 系统：WSL2 Ubuntu 24.04（内核 6.6.114），编译器 g++ 13.3.0
- 服务器线程数 = `hardware_concurrency() - 1` = 31；服务器与压测客户端同机（回环网络）
- 压测工具：webbench 1.5；服务器与客户端均先 `ulimit -n 65535`

示例输出（1000 并发）：

```bash
$ webbench -c 1000 -t 5 http://127.0.0.1:1234/
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://127.0.0.1:1234/
1000 clients, running 5 sec.

Speed=1586760 pages/min, 30201104 bytes/sec.
Requests: 132230 susceed, 0 failed.
```

全部实测数据：

| 目标 | 并发客户端 | Speed (pages/min) | 吞吐 | 请求总数 | 失败 |
|---|---|---|---|---|---|
| `/` 静态页 | 100 | 1,539,228 | 29.3 MB/s | 128,269 | 0 |
| `/` 静态页 | 1000 | 1,586,760 | 30.2 MB/s | 132,230 | 0 |
| `/` 静态页 | 5000 | 1,579,500 | 30.1 MB/s | 131,625 | 0 |
| `/` 静态页 | 8000 | 1,088,784 | 20.7 MB/s | 90,732 | 0 |
| `/fileserver` 动态页 | 1000 | 785,076 | 24.5 MB/s | 65,423 | 0 |

说明：

- 单机回环下 1000～5000 并发稳定在约 **2.6 万 QPS**，0 失败；8000 并发时压测客户端侧开销开始成为瓶颈，数据回落。
- 教程演示用的 `-c 10000` 参数在本机环境未能完成：webbench 客户端 fork 到第 9238 个进程时报
  `fork failed: Resource temporarily unavailable`（WSL 内存配额限制导致，属压测客户端侧限制，与服务器无关）。

## 已知限制

- demo 主程序仅绑定 `127.0.0.1`，如需外部访问请修改 `test/http_server.cpp` 中的监听地址。
- 异步日志已实现但 demo 默认关闭（`http_server.cpp` 中有对应注释，取消注释即可启用）。
- `/login` 为教程自带的演示路由。

## 致谢

- 教程：[Wlgls/30daysCppWebServer](https://github.com/Wlgls/30daysCppWebServer)
- 压测工具：[WebBench](https://github.com/EZLippi/WebBench)（Radim Kolar, 1997-2004）
