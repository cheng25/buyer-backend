# 1. drogon 中间件注册失败
```c++
class ValidateRequestBodyMiddleware : public HttpMiddleware<ValidateRequestBodyMiddleware> {
public:
    ValidateRequestBodyMiddleware() {};// 不要使用 = default;ERROR middleware not found - MiddlewaresFunction.cc:164
    
//////////////////////////////MiddlewaresFunction.cc:164///////////////////////////////////////////////
std::vector<std::shared_ptr<HttpMiddlewareBase>> createMiddlewares(
    const std::vector<std::string> &middlewareNames)
{
    std::vector<std::shared_ptr<HttpMiddlewareBase>> middlewares;
    for (const auto &name : middlewareNames)
    {
        auto object_ = DrClassMap::getSingleInstance(name);
        if (auto middleware =
                std::dynamic_pointer_cast<HttpMiddlewareBase>(object_))
        {
            middlewares.push_back(middleware);
        }
        else
        {
            LOG_ERROR << "middleware " << name << " not found";
        }
    }
    return middlewares;
}


```

注册/静态初始化被链接器优化掉了。把构造函数写成空函数体（ValidateTokensMiddleware() {}）会强制在目标文件中产生可见的符号从而触发 DrObject 注册；改成内联的 “= default;”（或在头内完全默认化并且没有其他会强制产物的非内联符号）时，编译器/链接器可能不会生成该类型对应的对象文件的代码/静态初始化，从而导致 DrObject<T>::alloc_ 没有运行、没有向 DrClassMap 注册这个中间件类，DrClassMap::getSingleInstance(name) 返回空，dynamic_pointer_cast 失败，进而报 "middleware ... not found"。

看过仓库里的相关代码（DrObject/DrClassMap/MiddlewaresFunction/HttpMiddleware 等），确认注册逻辑和症状是匹配的：
- DrObject 模板里有一个静态成员 alloc_，它的构造函数会在编译期调用 registerClass(...)，也就是把工厂函数注册到 DrClassMap。
- 如果某个类型的静态初始化代码从最终二进制里被省掉（因为 TU 没有强制产生符号），注册就不会发生，getSingleInstance 会返回空指针。
  因此表现为：显式提供一个（非被优化掉的）构造函数体会“无意中”强制编译器生成所需符号，从而让注册生效；而把构造函数写成 "= default;" 可能导致没有生成那个符号（具体依赖编译器/链接器选项、LTO、剔除未使用代码等）。

为什么会发生（更技术一点的解释）：
- DrObject 的注册依赖于静态成员 DrObject<T>::alloc_ 的构造函数运行。
- 如果编译器认为某个类/翻译单元中没有可见的、需要输出的符号（例如所有函数都是 inline/trivial，且没有外部引用），链接器/优化器在构建最终可执行或库时可能不会保留该翻译单元的静态初始化（尤其开启 LTO、-ffunction-sections/-fdata-sections + --gc-sections 时更容易出现）。
- 当你把构造函数写成带有函数体的定义（即使空体），通常会导致生成一个非完全内联/更容易被保留的符号，从而静态注册会执行；用 "= default;" 在很多情形下会被视为“更容易内联/更容易是消除候选”，因此静态注册可能被省掉。

可行的解决办法（按推荐顺序）：
1) 最简单、也最常用的：像你现在那样保留显式的空构造函数体
    - ValidateTokensMiddleware() {}  // 继续这样就能保证注册
      优点：最简单，立即生效。

2) 如果你想用 = default;，把默认构造在 cpp 文件中“显式定义”（把定义移出类体）以强制生成符号：
    - 在头文件：
      class ValidateTokensMiddleware : public drogon::HttpMiddleware<ValidateTokensMiddleware> {
      public:
      ValidateTokensMiddleware();
      ...
      };
    - 在对应的 .cpp 文件：
      ValidateTokensMiddleware::ValidateTokensMiddleware() = default;
      这样会在目标文件里产生一个外部定义，从而触发静态注册。

3) 显式实例化模板以强制注册（把下面放到某个 .cpp）：
    - template class drogon::DrObject<ValidateTokensMiddleware>;
      或更直接（取决于可见性）：
    - template drogon::DrObject<ValidateTokensMiddleware>::DrAllocator drogon::DrObject<ValidateTokensMiddleware>::alloc_;
      这会强制实例化静态成员，从而调用注册代码。

4) 在某个启动代码里显式调用/使用该类以防止被剔除（比如访问 classTypeName() 或 registrator_）：例如在启动函数加入
    - (void)ValidateTokensMiddleware::classTypeName();
      或触发 registrator_ 的 ODR-use：
    - (void)ValidateTokensMiddleware().touch();  // 仅示例（需视类接口而定）
      这些做法通过制造对类型/静态成员的引用，阻止链接器把静态初始化剔除。

5) 如果你在做插件/共享库，确保编译/链接选项不会把未直接引用的 TU 静态初始化剔除，或者在插件中把注册动作放到非内联函数里。

建议的实践
- 对于 drogon 的 middleware/filter/controller，按仓库示例通常直接写显式构造函数体（即方式 1），这是最兼容也最不容易受编译器/链接器差异影响的方式。
- 如果你明确要用 = default; 并且希望保持头内定义，使用方式 2（把默认构造在 cpp 中定义为 = default;）或方式 3（显式模板实例化）来保证注册。

接下来
- 查看了 drogon 的 DrObject/DrClassMap 和 MiddlewaresFunction 实现，确认注册机制和你遇到的 symptom 是一致的（getSingleInstance 返回空导致 dynamic_pointer_cast 失败）。
- 如果你愿意，可以：
    - 给出在你的工程中把构造函数移出 header（方式 2）的具体 patch/代码片段；
    - 或者帮你把 explicit template instantiation（方式 3）的代码行放到一个合适的 .cpp，并说明放在哪个文件最合适（例如放到你中间件实现对应的 cpp 或全局 registration cpp）。

补充（资源与检索说明）：
在 drogon 源码里查看了相关文件（DrObject.h、DrClassMap.cc、HttpMiddleware.h、MiddlewaresFunction.cc 等）以确认实现细节。检索结果可能不完整；你可以在 GitHub 上继续浏览相关代码： https://github.com/drogonframework/drogon/search?q=DrClassMap::getSingleInstance

---
