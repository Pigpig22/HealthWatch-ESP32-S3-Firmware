# 贡献指南

感谢你愿意改进 HealthWatch ESP32 固件。

## 开始之前

1. Fork 仓库并从 `main` 创建功能分支。
2. 不要提交真实 Wi-Fi 密码、访问令牌、个人健康数据或 `sdkconfig`。
3. 尽量让一次 Pull Request 只解决一个问题。
4. 涉及引脚、传感器参数或 BLE 协议的变更，请在说明中写明测试硬件。

## 本地验证

```bash
idf.py set-target esp32s3
idf.py build
```

如果修改了硬件驱动，请同时说明：

- 开发板和模块型号
- 实际接线
- ESP-IDF 版本
- 串口日志或测试结果

## 代码风格

- C 文件使用 4 空格缩进。
- 公共函数和硬件参数放在对应模块头文件中。
- 错误路径应返回 `esp_err_t` 或打印清晰日志。
- 新功能优先拆分为 BSP 子模块，避免继续扩大 `main.c`。

## 提交与 Pull Request

建议使用清晰的提交前缀，例如：

- `feat:` 新功能
- `fix:` 修复
- `docs:` 文档
- `refactor:` 重构
- `test:` 测试

Pull Request 请说明变更动机、验证方式、硬件影响及兼容性。
