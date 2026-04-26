# asmr-helper
这是一个更换视频源的工具

## 核心组件
调用ffmpeg,封装成一个程序
## 语言
可以使用c++做底层，同时提供typescripe为后续的gui界面做预备
不要依赖cmake库
## 功能
- 输入一个视频，调用mcp服务器/用指定API 查找一张图片，然后将画面换成该图片，音频保持不变
- 输入一段音频，同样的方式查找一张图片，输出一段视频
### API
在pexels获取壁纸类图片（开发文档https://www.pexels.com/api/d  
  ocumentation/；API密钥：sEgMIfajDuYkADrF3sfyAuT0vptn1tl1hdswMQi7fJYEHHxbsLexKNPp），你可以
  创建一个subagent帮助你阅读文档，程序完成后需要agent测试

## 要求
程序代码完成后，用测试agent进行测试
程序的工作目录要求在程序的文件夹下
### 程序封装
先暂时打包成cli程序，要求程序运行后有引导界面
将ffmpeg封装，使其他用户无需配置环境即可使用
准备两套版本（Windows和Linux）

## 测试
### 样例
视频文件位于/mnt/e/Telegram Desktop/test.mp4       ，图片url为https://img-s.msn.cn/tenant/amp/entityid/AA21qn2c.img?w=768&h=1039&m=6 