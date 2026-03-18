# UET2APlugin

基于混元 Motion API 的 Unreal Engine 文生动画插件。

当前版本聚焦一条纯运行时链路：

**文本提示词 → 提交任务 → 轮询状态 → 下载 FBX → 导入动画

插件只负责生成并返回动画对象，播放和业务消费由项目侧自行处理。

## 适用版本
- 已在 **Unreal Engine 5.7.4** 下联调
- 支持平台：`Win64`、`Mac`、`Linux`

## 主要功能
- 调用混元 Motion API 生成动作
- 异步轮询任务状态
- 下载并运行时导入 FBX
- 返回导入动画

## 安装
推荐作为项目插件放入：

```
<YourProject>/Plugins/UET2APlugin
```

打开项目后如提示编译模块，选择 **Yes**。

## 使用前准备
1. 混元 Motion API Key
2. 可编译 C++ 模块的 Unreal 项目

## 快速开始

### 蓝图接入
先调用：
- `Set T2A API Key`

如需自定义接口地址，再调用：
- `Set T2A Base URL`

默认 Base URL：
```text
http://api.taiji.woa.com
```

然后使用异步节点：
- `Generate Motion from Text`

#### 输入参数
| 参数 | 说明 |
|---|---|
| `Text Prompt` | 动作描述文本 |
| `Duration` | 动画时长，范围 `1~12` 秒 |

#### 输出事件
`OnProgress`、`OnCompleted`、`OnFailed` 现在使用**同一套数据引脚**，这样蓝图里能稳定看到完整输出。

| 引脚 | 类型 | 说明 |
|---|---|---|
| `Stage` | 枚举 | 当前阶段 |
| `Percent` | 浮点 | 当前进度 |
| `Status Message` | 字符串 | 当前状态描述 |
| `Imported Animation` | `UAnimSequence*` | 导入后的动画对象，仅 `OnCompleted` 时有效 |
| `Rewritten Prompt` | 字符串 | 实际送去生成的 Prompt，仅 `OnCompleted` 时有效 |
| `Error Message` | 字符串 | 错误信息，仅 `OnFailed` 时有效 |

#### 推荐接法
- `OnProgress`：更新 UI 或日志
- `OnCompleted`：读取 `Imported Animation`，交给你的播放或缓存逻辑
- `OnFailed`：显示 `Error Message`

## 编辑器面板
在 Unreal Editor 中打开：

```text
Window > Text-to-Animation
```

面板支持：
- 输入 API Key
- 输入动作描述
- 导入本地 FBX 做测试
- 查看进度和结果摘要

使用面板前请先进入 PIE，否则拿不到运行时子系统。

## 运行流程
1. 提交生成任务
2. 轮询任务状态
3. 获取 FBX URL
4. 下载 FBX 到临时目录
5. 导入为临时 `UAnimSequence`
6. 通过事件回传结果

## 临时文件目录
默认下载目录：

```text
<Project>/Saved/HunyuanMotion/Downloads
```

清理接口：
- `UFBXDownloader::CleanupTempFiles()`

## C++ 示例
```cpp
UT2AAnimationSubsystem* T2A = GetGameInstance()->GetSubsystem<UT2AAnimationSubsystem>();
if (!T2A)
{
    return;
}

T2A->SetAPIKey(TEXT("your-api-key"));

FT2APipelineConfig Config;
Config.TextPrompt = TEXT("A character walks forward and waves hello");
Config.Duration = 5;

T2A->OnPipelineCompleted.AddLambda(
    [](UAnimSequence* ImportedAnimation, const FString& FinalPrompt)
    {
        if (!ImportedAnimation)
        {
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("Imported animation: %s, prompt: %s"), *ImportedAnimation->GetName(), *FinalPrompt);
    });

T2A->RunPipeline(Config);
```

## 注意事项
- API Key / Base URL 当前不会自动持久化
- 运行时导入得到的动画对象位于 `Transient` 包中，不会自动保存为 `.uasset`
- 插件不会自动播放动画，播放时机由业务侧决定
- 依赖引擎内置 FBX SDK，若引擎裁剪掉该依赖则运行时导入不可用

## 故障排查

### 面板里提示 Subsystem not available
先进入 PIE，再回到面板点击 `Save` 或 `Generate Animation`。

### 生成成功但看不到可保存资产
当前返回的是运行时临时对象，默认不会出现在 Content Browser。

## 目录说明
- `Source/UET2APlugin`：运行时核心代码
- `Source/UET2APluginEditor`：编辑器面板与菜单入口
- `UET2APlugin.uplugin`：插件描述与依赖配置
