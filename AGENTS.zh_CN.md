<p align="right">
  <strong>简体中文</strong> · <a href="AGENTS.md">English</a>
</p>

# AI Agent 仓库规范

本文是 `ai-passport` AI 辅助工作的唯一必读入口。按下方路由表只读当前任务所需文档，不预读全部 README。详细开发工作流见 [docs/development/ai-guide.zh_CN.md](docs/development/ai-guide.zh_CN.md)。

## 必需 AI 技能

必需技能（`skills/`）：`passport-develop`、`passport-setup`、`passport-build`、`passport-device-test`、`passport-debug`。开发前 AI 须自检五个技能是否可用；缺失时在遵守环境审批与保留现有配置前提下自动安装并验证（受阻时如实说明，不谎报成功）。每次任务只加载匹配技能；烧录、Git 写操作和发布仍需单独授权。

## 项目与安全基线

- **平台与分区**：ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF 5.5.3。默认 `partitions.csv` 仅含 NVS、PHY 与单 factory app；自定义分区须验证。
- **工作区与烧录**：先执行 `git status --short --branch` 保留已有修改。烧录新固件无需先读取 Flash 备份（见[烧录与数据说明](docs/development/engineering/firmware-layout.zh_CN.md#烧录与已存数据)）。
- **事实优先级**：规格与实测 → `components/bsp/include/bsp_pins.h` → BSP 实现 → 硬件指南 → README/demo；缺失时询问用户，不猜测。
- **分层与 UI**：板级复用逻辑入 `components/bsp`，页面/状态机/应用任务入 `main`。二次开发应用必须重新设计独立 UI（见[二次开发 UI 强制重新设计规则](docs/development/ai-guide.zh_CN.md#二次开发-ui-强制重新设计)）。
- **运行时与字体**：LVGL 任务外访问对象须持 `bsp_lvgl_lock()`；按键回调禁阻塞；删除 screen 前停止相关任务/定时器/回调；可测试逻辑与 ESP-IDF/LVGL 解耦并用 host tests 覆盖；中文 UI 遵循[字体检查清单](docs/development/engineering/coding-conventions.zh_CN.md#中文字体与缺字排查)。
- **安全与双语**：禁提交凭证、二维码秘密或未脱敏日志。默认 `.md` 用英文，配对 `.zh_CN.md` 用简体中文并保留互链。

## 按任务加载上下文

| 任务 | 修改前读取 |
| --- | --- |
| 任意代码修改 | `docs/development/ai-guide.zh_CN.md`、相关头文件与相邻实现 |
| 技能配置或应用工作流 | `skills/README.zh_CN.md`，仅加载匹配当前任务的技能 |
| 环境引导或工具链 | `docs/development/engineering/environment-setup.zh_CN.md` |
| BSP / 硬件外设 | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`、`components/bsp/include/bsp_pins.h` |
| Demo 或菜单 | `main/demo.h`、`main/main.c`、最近的 `main/demo_*.c` |
| 中文 UI 或字体 | `docs/development/engineering/lvgl-chinese-fonts.zh_CN.md` |
| Wi-Fi 或蓝牙配网 | `docs/development/engineering/wifi-provisioning.zh_CN.md` |
| 构建、测试、分区 | `docs/development/engineering/build-and-test.zh_CN.md`、`docs/development/engineering/firmware-layout.zh_CN.md`、`sdkconfig.defaults`、`partitions.csv` |
| CI、发布、项目完成 | `docs/development/ci/CI-*.zh_CN.md`、`.github/workflows/`、`docs/development/release/project-completion.zh_CN.md` |
| 文档、Commit 或 PR | `docs/README.zh_CN.md`、`docs/contribution/doc-conventions.zh_CN.md`、`docs/contribution/commit-and-pr.zh_CN.md` |

## 必须执行的验证与交付格式

交付前依次运行 `./tools/validate.sh --static`、`--firmware` 及完整 `./tools/validate.sh`（需 ESP-IDF 5.5.3），并分别报告 `Build`、`Host tests`、`Device tests`（`PASS / FAIL / NOT RUN`）与 `Unverified`。完成固件修改后按[真机测试交接流程](docs/development/ai-guide.zh_CN.md#主动询问真机测试)主动询问并获批后再烧录。仅在用户请求时 commit/push；普通 PR 不改 `docs/CHANGELOG*.md`。社区规范见 `.github/`。
