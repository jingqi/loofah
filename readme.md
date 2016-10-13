Loofah
===
跨平台异步网络库

## 目前的实现：

### Reactor

| 平台 | 实现 |
|-|-|
| Windows  | - |
| MacOS | kqueue |
| Linux | epoll |

### Proactor

| 平台 | 实现 |
|-|-|
| Windows | CompletionPort |
| MacOS | - |
| Linux | epoll(模拟) |
