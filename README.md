## 简介

该组件基于平头哥YoC平台使用wma以支持aac编解码功能。

wmafixed是一个定点化的开源wma解码库，支持WMAV1和WMAV2版本

项目链接：https://code.aliyun.com/jingzhishen/yoc_wma

## 如何在YoC平台下编译使用

- 将wma编解码库拷贝到YoC components文件夹下
- 将ad_wma.c拷贝到components/av/avcodec文件夹下
- 修改av组件中的package.yaml文件，将该文件加入到源文件编译列表中。同时在depends项中加入wma依赖。
- 修改components/av/avcodec/avcodec_all.c，添加如下代码：

```c
/**
 * @brief  regist ad for wma
 * @return 0/-1
 */
int ad_register_wma()
{
    extern struct ad_ops ad_ops_wma;
    return ad_ops_register(&ad_ops_wma);
}
```

- 修改components/av/include/avcodec/avcodec_all.h，加入该函数的头文件声明。并修改ad_register_all内联函数，加入wma解码支持，代码如下：

```c
#if defined(CONFIG_DECODER_WMA)
    REGISTER_DECODER(WMA, wma);
#endif
```

- 修改solutions/pangu_demo/package.yaml，若使能wma解码，需加入如下配置项：

```c
CONFIG_DECODER_WMA: 1
```

