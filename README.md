# UET2APlugin

基于混元 Motion API 的 Unreal Engine 文生动画插件。

它提供一条完整的运行时流水线：

**文本提示词 → 提交生成任务 → 轮询结果 → 下载 FBX → 运行时导入动画 → 重定向到目标骨骼 → 可选自动播放**

## 适用版本
- 已在 **Unreal Engine 5.7.4** 下完成编译验证
- 支持平台：`Win64`、`Mac`、`Linux`

## 主要功能
- 运行时调用混元 Motion API 生成动作
- 异步轮询任务状态
- 下载生成得到的 FBX 文件
- 使用运行时 FBX 导入器构建 `UAnimSequence`
- 将动画重定向到目标角色骨骼
- 提供蓝图异步节点，便于直接在项目中使用
- 提供编辑器调试面板，便于联调 API 和流水线状态

## 插件结构
本插件包含两个模块：
- `UET2APlugin`：运行时模块
- `UET2APluginEditor`：编辑器模块

依赖项：
- `IKRig` 插件
- 引擎自带 `HTTP` / `Json` / `JsonUtilities`
- 引擎 ThirdParty `FBX` SDK（标准 UE 发行版通常已包含）

## 安装方式

### 方式一：作为项目插件安装（推荐）
1. 关闭 Unreal Editor。
2. 将整个插件目录复制到你的项目目录：
   ```text
   <YourProject>/Plugins/UET2APlugin
   ```
3. 确认项目目录中最终存在：
   ```text
   <YourProject>/Plugins/UET2APlugin/UET2APlugin.uplugin
   ```
4. 打开项目。
5. 如果 Unreal 提示需要编译模块，选择 **Yes**。
6. 确认 `IKRig` 已启用：
   - `Edit > Plugins`
   - 搜索 `IKRig`
   - 勾选启用后重启编辑器

### 方式二：作为引擎插件安装
也可以放到引擎插件目录中，但更推荐按项目安装，便于版本跟随项目管理。

## 使用前准备
在真正调用生成前，请先准备：

1. **混元 Motion API Key**
2. 可运行 C++ 模块的 Unreal 项目环境
3. 一个目标角色的 `SkeletalMeshComponent`（如果你希望直接把动画播放到角色身上）
4. 如需更好的骨骼适配，准备一个 `IK Retargeter` 资源（可选）

## 快速开始

### 方案 A：蓝图里直接使用（推荐）
这是当前最适合业务接入的方式。

#### 第一步：设置 API Key
在蓝图里先调用：
- `Set T2A API Key`

如需切换接口地址，再调用：
- `Set T2A Base URL`

默认 Base URL 为：
```text
http://api.taiji.woa.com
```

#### 第二步：调用生成节点
使用异步蓝图节点：
- `Generate Motion from Text`

该节点会完成整条流水线。

#### 节点参数说明
| 参数 | 说明 |
|---|---|
| `Text Prompt` | 动作描述文本 |
| `Duration` | 动画时长，范围 `1~12` 秒 |
| `Target Character` | 目标角色的 `SkeletalMeshComponent`，可为空 |
| `Retarget Asset` | `IKRetargeter` 资源，可为空 |
| `bAutoPlay` | 生成完成后是否自动播放 |
| `bLooping` | 自动播放时是否循环 |

#### 输出事件
| 事件 | 说明 |
|---|---|
| `OnProgress` | 返回当前阶段、进度百分比、状态文本 |
| `OnCompleted` | 返回生成完成的 `UAnimSequence` 和实际使用的 Prompt |
| `OnFailed` | 返回错误信息 |

#### 推荐蓝图接法
1. BeginPlay 或用户点击按钮后，先调用 `Set T2A API Key`
2. 调用 `Generate Motion from Text`
3. 把角色的 `Mesh` 组件接到 `Target Character`
4. 绑定：
   - `OnProgress`：更新 UI
   - `OnCompleted`：提示成功 / 缓存动画引用
   - `OnFailed`：显示错误信息

如果传入了 `Target Character` 且 `bAutoPlay = true`，插件会在完成后直接对该组件调用 `PlayAnimation()`。

---

### 方案 B：编辑器面板调试
插件提供了一个编辑器面板，适合做接口联调和流水线检查。

#### 打开方式
在 Unreal Editor 菜单中打开：
```text
Window > Text-to-Animation
```

打开后会出现面板：
- 输入 API Key
- 输入动作描述
- 设置 Duration
- 选择是否禁用 Prompt Rewrite
- 点击 `Generate Animation`

#### 重要说明
当前编辑器面板是通过 **PIE World** 获取 `UT2AAnimationSubsystem`，所以：

1. **请先启动 PIE / Play In Editor**
2. 再打开面板或回到面板中点击 `Save`
3. 然后再点 `Generate Animation`

如果没先进入 PIE，面板会拿不到运行时子系统，无法真正执行流水线。

## 运行时工作流程
插件内部实际执行流程如下：

1. 提交文本生成任务
2. 轮询任务状态
3. 获取返回的 FBX URL
4. 下载 FBX 到本地临时目录
5. 运行时解析 FBX，构建临时 `USkeleton` 和 `UAnimSequence`
6. 如有目标角色，则尝试进行骨骼兼容性判断与重定向
7. 如配置了自动播放，则播放到目标角色上

## 下载目录与临时文件
FBX 下载文件默认保存在：
```text
<Project>/Saved/HunyuanMotion/Downloads
```

插件提供了临时文件清理接口：
- `UFBXDownloader::CleanupTempFiles()`

## C++ 用法示例
如果你想直接在 C++ 里调用，可以使用 `UT2AAnimationSubsystem`：

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
Config.TargetCharacter = GetMesh();
Config.bAutoPlay = true;
Config.bLooping = false;

T2A->RunPipeline(Config);
```

常用接口：
- `SetAPIKey()`
- `SetBaseURL()`
- `RunPipeline()`
- `CancelPipeline()`
- `GetCurrentStage()`
- `IsRunning()`

常用事件：
- `OnPipelineProgress`
- `OnPipelineCompleted`
- `OnPipelineFailed`

## 已知限制与注意事项

### 1. API Key / Base URL 当前不会自动持久化
当前版本没有把 API Key 或 Base URL 写入配置文件。
这意味着：
- 重新启动编辑器后需要重新设置
- 新的运行会话（PIE）开始后，建议重新确认配置

### 2. 编辑器面板更适合调试，不是完整内容生产工具
当前面板实现里：
- 不会给目标角色自动播放动画
- 没有暴露目标角色选择 UI
- 主要用于验证 API、下载、导入与状态更新流程

如果你想在项目里真正驱动角色动作，优先使用蓝图异步节点或 C++ 子系统接口。

### 3. 生成出的动画对象是临时对象
运行时导入得到的 `USkeleton` / `UAnimSequence` 当前创建在 `Transient` 包里：
- 不会自动出现在 Content Browser
- 不会自动保存成 `.uasset`
- 更适合运行时消费，而不是编辑器内长期资产沉淀

### 4. IK Retargeter 资源当前不会完整走运行时 IK Processor 流程
当前实现会优先尝试 `RetargetAsset`，但由于运行时初始化条件限制，实际上会回退到**骨骼名自动映射**方案。
所以：
- `RetargetAsset` 目前更像是预留入口
- 真正的重定向结果主要依赖骨骼结构与命名相似度
- 目标骨骼和源骨骼命名越接近，效果通常越稳定

### 5. 依赖引擎内置 FBX SDK
如果你使用的是被裁剪过的引擎版本，且缺少引擎 ThirdParty FBX SDK，则运行时 FBX 导入会不可用。
标准 UE 5.7.4 安装通常不需要额外处理。

## 故障排查

### 打开项目时提示插件编译失败
请确认：
- 当前使用的是 UE 5.7.4 或兼容版本
- `IKRig` 已启用
- 本机已安装对应平台的 C++ 编译环境
- 项目不是把备份目录也一起放进了 `Plugins/` 下

### 面板里点击 Generate 没反应 / 提示 Subsystem not available
原因通常是：
- 还没进入 PIE

处理方式：
- 先点击 Play 进入 PIE
- 再回到面板点 `Save` / `Generate Animation`

### 生成成功但角色没动
请检查：
- 是否给 `Target Character` 传入了有效的 `SkeletalMeshComponent`
- 是否开启了 `bAutoPlay`
- 目标 Mesh 是否有有效 Skeleton
- 源骨骼和目标骨骼是否差异过大

### 生成成功但看不到可保存的动画资产
这是当前版本的设计现状：
- 动画是运行时临时对象
- 默认不会自动保存到 Content Browser

## 建议接入方式
如果你的目标是“项目里角色直接动起来”，推荐优先顺序：

1. **蓝图异步节点接入**
2. **C++ 子系统接入**
3. **编辑器面板仅用于联调**

## 目录说明
- `Source/UET2APlugin`：运行时核心代码
- `Source/UET2APluginEditor`：编辑器面板与菜单入口
- `UET2APlugin.uplugin`：插件描述与依赖配置

## 后续可扩展方向
如果后续继续完善，这几个方向最值得做：
- 将 API Key / Base URL 持久化到配置文件
- 支持把生成动画保存为真实 `uasset`
- 在编辑器面板中支持选择目标角色与 Retarget 资源
- 改进运行时 IK Retargeter 支持
- 增加更完整的错误码与用户提示
