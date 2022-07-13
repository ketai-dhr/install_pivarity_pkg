## 说明
**install_pivariety_pkgs.sh** is the main running program
现在支持主流的bulleye,buster

### 需要做的事情
1. camera_i2c 工具需要更新,也可以不更新,但是 **gpio** 这个函数已经被官方抛弃（已解决）
2. rpi3-gpiovirtbuf 需要 32位和64位的进行区分 （已解决）

### 存在问题(日后可以优化)
1. 暂时不支持 CM3,4 等
2. imx519降速暂时没有 5.17 以下的内核版本
