# UET2APlugin

基于混元 Motion API 的 Unreal Engine 文生动画插件。

当前版本同时提供两条接入方式：

- **运行时 / 蓝图链路**：文本提示词 → 提交任务 → 轮询状态 → 下载 FBX → 导入动画
- **编辑器面板链路**：在编辑器态直接生成并导入动画资产到 Content Browser

运行时链路负责生成并返回动画对象；编辑器面板链路负责直接落盘为项目资产。动画的播放和业务消费仍由项目侧自行处理。

## 适用版本
- 已在 **Unreal Engine 5.6、5.7** 下联调
- 支持平台：`Win64`、`Mac`
## 主要功能
- 调用混元 Motion API 生成动作
- 异步轮询任务状态
- 下载并导入 FBX 动画
- 可选复用目标 `SkeletalMesh` 的 `Skeleton`
- 支持在编辑器态直接生成并保存动画资产到 Content Browser
- 运行时链路与编辑器面板复用同一套核心导入链路

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
| `TargetSkeletalMesh` | 可选。传入后会优先复用该 `SkeletalMesh` 的 `Skeleton` 来导入动画 |

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

#### 导入行为说明
- 如果传入 `TargetSkeletalMesh`，导入器会优先复用它的 `Skeleton`
- 如果 FBX 与目标骨架只有部分骨骼匹配，缺失轨道会被跳过并输出 warning
- 如果根骨或骨架完全不兼容，导入会直接失败并返回错误
- 在编辑器环境下，默认会把导入出的动画保存到：`/Game/HunyuanMotion/Imported`
- 在非编辑器环境下无法写入 Content Browser，会自动回退为 `Transient` 动画对象

## 编辑器面板
在 Unreal Editor 中打开：

```text
Window > Text-to-Animation
```

面板用于在 **编辑器态** 直接生成并导入 T2A 动画资产到项目目录，**不依赖 PIE，也不需要运行时子系统**。

### 面板输入
- `API Key`
- `Animation Prompt`
- `Duration`（范围 `1~12` 秒）
- `Target SkeletalMesh`（可选）
- `Import Path`（例如 `/Game/HunyuanMotion/Imported`）

### 使用流程
1. 在顶部填写 `API Key`，点击 `Apply`
2. 输入动作描述、时长和目标导入目录
3. 如需复用现有骨架，选择 `Target SkeletalMesh`
4. 点击 `Generate and Import Asset`
5. 在状态区查看提交、生成、下载、导入进度；如有需要可点击 `Cancel`

### 导入结果
- 选择了 `Target SkeletalMesh` 时，会优先复用其 `Skeleton` 导入动画资源
- 未选择时，会按 FBX 骨架创建新的 `Skeleton` 与动画资源
- `Import Path` 不存在时，会按给定 Unreal 资源路径创建并保存资产
- 成功后状态栏会显示导入结果摘要；如果接口返回了改写后的 Prompt，也会一并展示

## 运行流程
1. 提交生成任务
2. 轮询任务状态
3. 获取 FBX URL
4. 下载 FBX 到临时目录
5. 导入为 `UAnimSequence`（编辑器下优先保存为资产，其他环境回退为临时对象）
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
Config.TargetSkeletalMesh = MyCharacterSkeletalMesh; // 可选：复用角色现有 Skeleton
Config.bSaveImportedAssetsToContent = true;          // 编辑器下默认就是 true
Config.OutputAssetFolder = TEXT("/Game/HunyuanMotion/Imported");

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
- 编辑器环境下默认会尝试把导入结果保存为 `.uasset`；如果保存不可用或在非编辑器环境下，则会回退成 `Transient` 动画对象
- 插件不会自动播放动画，播放时机由业务侧决定
- 传入 `TargetSkeletalMesh` 时，FBX 骨架需要与目标骨架至少具备基础兼容性，否则导入会失败
- 依赖引擎内置 FBX SDK，若引擎裁剪掉该依赖则运行时导入不可用

## 故障排查

### 面板里提示 API Key is not configured
先确认已经在面板顶部填写并应用了 API Key，然后再点击 `Generate and Import Asset`。

### 生成成功但看不到可保存资产
先检查 `Import Path` 是否为合法的 Unreal 资源路径（例如 `/Game/HunyuanMotion/Imported`）。
如果你选择的是编辑器面板流程，生成成功后动画应直接保存到 Content Browser 对应目录。

### 导入时报骨架不兼容
如果你传了 `TargetSkeletalMesh`，请确认：
- FBX 根骨名称存在于目标 `Skeleton`
- 主要骨骼命名与目标骨架基本一致
- 日志里没有出现 `does not share any bone names` 或 `missing FBX root bone` 这类错误

## 目录说明
- `Source/UET2APlugin`：运行时核心代码
- `Source/UET2APluginEditor`：编辑器面板与菜单入口
- `UET2APlugin.uplugin`：插件描述与依赖配置
