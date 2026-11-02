# AI Passport Agent 入口

本文件仅负责路由，避免多根工作区在会话启动时加载完整固件规则。
仅当任务明确涉及本仓库时，完整读取[详细规则](PROJECT_RULES.md)。
## Required AI skills
开发前必须读取[详细规则中的技能策略](PROJECT_RULES.md#required-ai-skills)，并只调用匹配当前任务的 Skill。
不要预读无关项目；保留用户已有改动并运行匹配风险的验证。
刷写、发布、凭据、提交与推送仍须遵守详细规则和用户授权。
