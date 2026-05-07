```bash

```

## 中文参考

[中文参考详细说明](https://zread.ai/uzoochogu/buyer-backend)

## 部署

```bash
#1. 切换目录到buyer-backend/docker-compose.yml
cd buyer-backend
# 1.1 修改docker-compose.yml配置文件
# 1.2 持久卷数据.volumes/改为 /data/usershare/buyer-backend/
# 1.3 加入已存在的自定义网络 my-net
# 1.4 网络声明 networks: 已经存在my-net

# 2. 拉取镜像
sudo docker pull postgis/postgis:18-3.6-alpine
sudo docker pull minio/minio:latest

# 3. 确保端口不被占用 5432、9000、9001
sudo netstat -tulnp | grep -E '5432|9000|9001'

#4. 启动容器
sudo docker compose up -d
# 或者
sudo docker-compose up -d

```

## 拉取

```bash
#全获取子模块
git submodule update --init --recursive
```

## 编译

1.CMake加载

```bash
# 在buyer-backend下
/usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MAKE_PROGRAM=/usr/bin/make -DCMAKE_C_COMPILER=/usr/bin/gcc-14 -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 -G "Unix Makefiles" -S ./ -B ./cmake-build-relwithdebinfo

# 或者使用绝对路径/install/drogon_dev/buyer-backend
/usr/bin/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MAKE_PROGRAM=/usr/bin/make -DCMAKE_C_COMPILER=/usr/bin/gcc-14 -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 -G "Unix Makefiles" -S /install/drogon_dev/buyer-backend -B /install/drogon_dev/buyer-backend/cmake-build-relwithdebinfo

```

2.构建

```bash
# 在buyer-backend下
/usr/bin/cmake --build ./cmake-build-relwithdebinfo --target buyer-backend -- -j 10

# 或者使用绝对路径/install/drogon_dev/buyer-backend
/usr/bin/cmake --build /install/drogon_dev/buyer-backend/cmake-build-relwithdebinfo --target buyer-backend -- -j 10

```

## 启动
1. 
```bash
#1. 复制config-sample.json配置文件并改名config.json
cp config-sample.json config.json
#2. 修改config.json中host 执行容器名称
#2.1 db_clients中"host": "buyer-backend-postgres-main",
#2.2 db_clients中"passwd": "mypassword",
#2.3 custom_config中"minio_endpoint": "http://buyer-backend-minio:9000",

#2. 启动服务
cd cmake-build-relwithdebinfo
./buyer-backend
```
2. 验证运行状态
打开浏览器或使用 curl 确认服务器正常响应。返回404 错误证明服务器正在运行。
```bash
curl -v http://localhost:5555/
```

可以访问 `Minio` 控制台，验证 `media` 存储桶是否已成功创建：
```bash
http://localhost:9001
# 登录凭据：minioadmin / mypassword
```

## 架构概览

该应用遵循经典的分层设计：底层为 PostgreSQL（结合 PostGIS 处理地理空间查询），上层为 Drogon HTTP 框架，同时由 ZeroMQ 处理实时的发布/订阅通知，并由 Minio 提供兼容 S3 的对象存储服务用于媒体文件管理。

```mermaid
graph TB
    subgraph Client
        CLI["HTTP Client / Frontend"]
        WS["WebSocket Client"]
    end

    subgraph "后端Buyer Backend (Drogon on : 5555)"
        MW["Middleware Layer<br/>Auth + CORS + WebSocket Auth"]
        CTL["Controllers<br/>Auth · Users · Offers · Chats<br/>Community · Orders · Search"]
        SM["ServiceManager 服务管理<br/>Singleton Orchestrator 单例协调器"]
    end

    subgraph Services 服务
        ZMQ_PUB["ZeroMQ Publisher 发布器"]
        ZMQ_SUB["ZeroMQ Subscriber 订阅者"]
        S3["AWS S3 Client → Minio"]
    end

    subgraph Infrastructure 基础设施
        PG[("PostgreSQL + PostGIS<br/>:5432")]
        MINIO["Minio Object Store<br/>:9000 / :9001"]
    end

    CLI -->|"REST API"| MW --> CTL --> SM
    WS -->|"WS Upgrade"| MW --> CTL
    CTL -->|"ORM / Coroutines"| PG
    SM --> ZMQ_PUB
    SM --> S3
    ZMQ_SUB -->|"Fan-out 扇出"| WS
    S3 -->|"Presigned URLs 预签名的 URL"| MINIO
    ZMQ_SUB -.->|"Read 读取"| ZMQ_PUB

    style PG fill:#336791,stroke:#fff,color:#fff
    style MINIO fill:#C72C48,stroke:#fff,color:#fff
    style SM fill:#f9f,stroke:#333,color:#000

```

## 项目结构

```bash
buyer-backend/
├── main.cc                         # 应用入口  
├── CMakeLists.txt                  # 构建配置 (C++23、依赖项、测试目标)
├── vcpkg.json                      # vcpkg 包管理器的依赖清单
├── config-sample.json              # config.json 的模板 (生产环境)
├── test_config-sample.json         # test_config.json 的模板 (测试环境)
├── Dockerfile                      # 基于 drogonframework/drogon 的容器镜像
├── docker-compose.yml              # PostgreSQL+PostGIS、Minio、可选的应用容器
├── docker-compose.test.yml         # 隔离的测试基础设施
│
├── controllers/                    # HTTP 和 WebSocket 端点处理器
│   ├── authentication.{hpp,cc}     # 登录、注册、登出、令牌刷新
│   ├── community.{hpp,cc}          # 帖子 CRUD、订阅、标签
│   ├── offers.{hpp,cc}             # 报价、协商、凭证、担保交易
│   ├── offers_negotiation.cc       # 协商业务逻辑
│   ├── chats.{hpp,cc}              # 会话和消息
│   ├── orders.{hpp,cc}             # 订单管理
│   ├── search.{hpp,cc}             # 结合 PostGIS 的全文搜索
│   ├── location.{hpp,cc}           # 地理空间聚类和附近搜索
│   ├── media_server.{hpp,cpp}      # 预签名 URL、媒体元数据(通过预签名 URL 进行媒体上传/下载)
│   ├── notification_websocket.{hpp,cpp}  # WebSocket 实时推送
│   ├── dashboard.{hpp,cc}          # 仪表盘聚合端点
│   ├── users.{hpp,cc}              # 用户列表
│   ├── common_req_n_resp.hpp       # 共享结构体 (SimpleError、StatusResponse 等)
│   ├── scenario_specific_utils.hpp  # 特定领域的辅助逻辑
│   └── controller_template.hpp     # 用于生成新控制器的模板
│
├── filters/                        # Drogon HTTP 中间件
│   ├── AuthMiddleware.cc           # JWT Bearer 令牌验证 (基于协程)
│   ├── CorsMiddleware.cc           # 跨域资源共享(回环强制执行 + CORS 请求头)
│   └── WebSocketAuthMiddleware.cc  # WebSocket 的 JWT 验证 (查询参数 / 请求头)
│
├── services/                       # 后台业务服务和基础设施 (S3, ZeroMQ pub/sub)
│   ├── service_manager.hpp         # 单例：初始化 ZeroMQ、S3、订阅者线程
│   ├── media_server/
│   │   └── s3_service.{hpp,cpp}    # AWS S3 客户端封装 (预签名 URL、存储桶管理)
│   └── subber/                     # 发布/订阅通知管道
│       ├── connection_manager.hpp  # WebSocket 连接注册表与主题广播
│       ├── pub_manager.hpp         # ZeroMQ PUB 套接字封装(inproc://pubsub)
│       ├── sub_manager.hpp         # ZeroMQ SUB 套接字 + 分发线程(jthread 轮询循环)
│       ├── redis_pub_manager.hpp   # Redis 替代方案 (桩代码，未使用)
│       └── redis_sub_manager.{hpp,cpp}  # Redis 替代方案 (桩代码，未使用)
│
├── utilities/                      # 可复用的纯函数辅助工具
│   ├── conversion.hpp              # 类型转换工具
│   ├── json_manipulation.hpp       # JSON 解析和构建辅助函数
│   ├── validation.hpp              # 输入验证函数
│   ├── time_manipulation.hpp       # 时间戳格式化和解析
│   └── uuid_generator.hpp          # 基于 Boost.UUID 的 ID 生成
│
├── migrations/                     # 数据库架构 (PostgreSQL + PostGIS),数据库 Schema (通过 Docker init 自动应用)
│   └── 001_complete_schema.sql     # 所有表：users、posts、offers、messages 等
├── seeds/                          # 用于开发的样本数据 ,示例种子数据 (通过 Docker init 自动应用)
│   └── 001_complete_seed_data.sql
├── test/                           # 集成测试套件
│   ├── CMakeLists.txt
│   ├── test_main.cc
│   ├── helpers.hpp
│   └── test_*.cc                   # 每个领域一个测试文件
│
└── config/
    └── config.hpp                  # 运行时配置访问器 (JWT 密钥等)

```

## 依赖


| 库                       | 作用                                             |
| ------------------------ | ------------------------------------------------ |
| drogon[ctl,orm,postgres] | HTTP 框架，包含 ORM 和 PostgreSQL 驱动           |
| jwt-cpp                  | JWT 令牌的创建与验证                             |
| argon2[hwopt,tool]       | 密码哈希（PHC 竞赛获胜算法）                     |
| cppzmq                   | 用于发布/订阅通知的 ZeroMQ C++ 绑定              |
| aws-sdk-cpp[s3]          | 用于 Minio 对象存储的 S3 客户端                  |
| boost-uuid               | 高性能 UUID 生成                                 |
| unordered-dense          | 优化版哈希映射 (Ankerl)                          |
| glaze                    | 基于反射的 JSON 序列化（比 JsonCpp 快约 2.1 倍） |

## 启动

```mermaid
flowchart TD
    A["Start buyer-backend<br/>启动 buyer-backend"] --> B["Parse CLI arguments<br/>解析命令行参数<br/>--config / --test / --help"]
    B --> C["Register HTTP listener<br/>注册 HTTP 监听器<br/>0.0.0.0:5555"]
    C --> D["Load config.json<br/>加载配置文件<br/>DB + App + Custom settings"]
    D --> E["ServiceManager::initialize<br/>服务管理器初始化<br/>ZeroMQ context + S3 client"]
    E --> F{"Minio 'media' bucket exists?<br/>Minio media 存储桶是否存在？"}
    F -->|Yes 是| G["Subscriber thread starts<br/>订阅者线程启动"]
    F -->|No 否| H["Create bucket → exit on failure<br/>创建存储桶 → 失败则退出"]
    H --> F
    G --> I["Drogon event loop runs<br/>Drogon 事件循环启动<br/>Server is live 🟢"]
```

## Drogon 启动流程
应用使用 `drogon::sync_wait` 运行一个协程，以确保在接收流量之前 S3 的 "media" 存储桶已存在。这在启动时将 Drogon 的协程基础设施与 AWS SDK 的异步接口连接起来，如果对象存储层不可达，则会快速失败。
```mermaid
flowchart TD
    A["解析命令行参数"] --> B{"存在 --test 标志?"}
    B -->|是| C["加载 test_config.json"]
    B -->|否| D{"存在 --config <file>?"}
    D -->|是| E["加载指定配置"]
    D -->|否| F["加载 config.json<br/>向上搜索 3 个父目录"]
    C --> G["初始化 ServiceManager<br/>ZeroMQ + AWS SDK + S3"]
    E --> G
    F --> G
    G --> H["通过 sync_wait 协程<br/>确保 S3 媒体存储桶存在"]
    H --> I["drogon::app().run()<br/>事件循环启动"]
    I --> J["退出时: ServiceManager.shutdown()"]
```

每个已认证端点的执行顺序始终为：CorsMiddleware跨域验证中间件 → AuthMiddleware认证 → 控制器处理程序。此顺序在每个 ADD_METHOD_TO 调用中声明，并由 Drogon 的框架调度强制执行。

## 架构概览
---

```mermaid
flowchart TB
    %% 定义样式类
    classDef client fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#0d47a1;
    classDef drogon fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#1b5e20;
    classDef controller fill:#fff9c4,stroke:#fbc02d,stroke-width:2px,color:#f57f17;
    classDef service fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px,color:#4a148c;
    classDef external fill:#ffccbc,stroke:#d84315,stroke-width:2px,color:#bf360c,stroke-dasharray: 5 5;

    %% 1. 客户端层
    subgraph Client["📱 客户端层"]
        direction TB
        HTTP["🌐 HTTP 客户端<br/>(REST API)"]:::client
        WS["🔌 WebSocket 客户端<br/>(实时通知)"]:::client
    end

    %% 2. Drogon 框架层
    subgraph Drogon["🚀 Drogon 框架层"]
        direction TB
        Router["🛣️ HTTP 路由"]:::drogon
        WSRouter["🔀 WebSocket 路由"]:::drogon
        CM["🛡️ CorsMiddleware"]:::drogon
        AM["🔑 AuthMiddleware<br/>(JWT HS256)"]:::drogon
        WAM["🔐 WebSocketAuthMiddleware<br/>(Query/Header)"]:::drogon
    end

    %% 3. 控制器层
    subgraph Controllers["⚙️ 控制器层 (api::v1)"]
        direction TB
        Auth["身份验证"]:::controller
        Community["社区<br/>(帖子)"]:::controller
        Offers["报价<br/>(协商/凭证)"]:::controller
        Chats["聊天<br/>(会话/消息)"]:::controller
        Search["搜索 / 位置"]:::controller
        Media["MediaController<br/>(预签名 URL)"]:::controller
        Other["用户/订单/仪表盘"]:::controller
        NotifWS["NotificationWebSocket"]:::controller
    end

    %% 4. 服务层
    subgraph Services["🧠 服务层 (Business Logic)"]
        direction TB
        SM["ServiceManager<br/>(单例/核心逻辑)"]:::service
        PM["PubManager<br/>(ZMQ PUB)"]:::service
        SubM["SubManager<br/>(ZMQ SUB + jthread)"]:::service
        CM2["ConnectionManager<br/>(WS 连接追踪)"]:::service
        S3["S3Service<br/>(AWS SDK)"]:::service
    end

    %% 5. 外部基础设施
    subgraph External["💾 外部基础设施"]
        direction TB
        PG[("🗄️ PostgreSQL<br/>+ PostGIS")]:::external
        S3Store[("☁️ S3 / MinIO")]:::external
    end

    %% --- 连接关系 ---

    %% HTTP 请求流
    HTTP ==> Router --> CM --> AM --> Controllers
    
    %% WebSocket 连接流
    WS ==> WSRouter --> WAM --> NotifWS

    %% 控制器内部调用
    Controllers -.-> SM
    Media --> S3
    
    %% 业务逻辑触发发布
    Community & Offers & Chats --> PM

    %% ZMQ 内部通信
    PM -- "inproc://pubsub" --> SubM

    %% 消息分发与持久化
    SubM --> CM2
    CM2 -.-> WS
    
    %% 数据持久化
    Controllers --> PG
    CM2 -.-> PG

    %% 文件存储
    S3 --> S3Store
```
---

```mermaid
flowchart TB
%% --- 样式定义 ---
    classDef client fill:#e3f2fd,stroke:#1565c0,color:#0d47a1,stroke-width:2px;
    classDef filter fill:#fff9c4,stroke:#fbc02d,color:#f57f17,stroke-width:2px;
    classDef controller fill:#c8e6c9,stroke:#2e7d32,color:#1b5e20,stroke-width:2px;
    classDef service fill:#f3e5f5,stroke:#7b1fa2,color:#4a148c,stroke-width:2px;
    classDef infra fill:#eeeeee,stroke:#616161,color:#212121,stroke-width:2px,stroke-dasharray: 5 5;

%% --- 节点定义 ---
    Client["🌐 HTTP Client<br/>HTTP 客户端"]:::client

    subgraph Filters["🛡️ 过滤器层 (filters/)"]
        direction TB
        CORS["CorsMiddleware<br/>(loopback-only guard)<br/>跨域中间件"]:::filter
        Auth["AuthMiddleware<br/>(JWT verification)<br/>认证中间件"]:::filter
    end

    Router["🛣️ Drogon Router<br/>Drogon 路由"]:::controller

    subgraph Handlers["⚙️ 控制器层 (controllers/)"]
        direction TB
        Ctrl["Controller Handler<br/>(drogon::Task<> coroutine)<br/>控制器处理函数"]:::controller
    end

    subgraph Services["🧠 服务层 (services/)"]
        direction TB
        S3Svc["ServiceManager::get_s3_service()<br/>S3 服务"]:::service
        PubSvc["ServiceManager::get_publisher()<br/>消息发布服务"]:::service
    end

    subgraph Infra["💾 基础设施"]
        direction TB
        DB[("🗄️ PostgreSQL<br/>数据库")]:::infra
    end

%% --- 连接关系 ---
%% 主请求流
    Client ==>|"1. HTTP Request<br/>HTTP 请求"| CORS
    CORS --> Auth
    Auth --> Router
    Router --> Ctrl

%% 内部处理
    Ctrl -->|"2. execSqlCoro<br/>执行SQL"| DB
    Ctrl -->|"3. media ops<br/>媒体操作"| S3Svc
    Ctrl -->|"4. notifications<br/>发送通知"| PubSvc

%% 响应流 (虚线表示回调)
    Ctrl -.->|"5. callback(resp)<br/>响应回调"| Client
```
---

| 控制器 | 头文件 | 基类 | 端点数量 | 需要认证 | 领域 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `Authentication` | [authentication.hpp](controllers/authentication.hpp) | `HttpController` | 4 | 混合 | 身份认证 |
| `Users` | [users.hpp](controllers/users.hpp) | `HttpController` | 1 | 是 | 用户资料 |
| `Community` | [community.hpp](controllers/community.hpp) | `HttpController` | 11 | 是 | 帖子与订阅 |
| `Offers` | [offers.hpp](controllers/offers.hpp) | `HttpController` | 22 | 是 | 交易市场报价 |
| `Chats` | [chats.hpp](controllers/chats.hpp) | `HttpController` | 7 | 是 | 消息传递 |
| `Orders` | [orders.hpp](controllers/orders.hpp) | `HttpController` | 2 | 是 | 订单管理 |
| `Dashboard` | [dashboard.hpp](controllers/dashboard.hpp) | `HttpController` | 1 | 是 | 数据分析 |
| `Search` | [search.hpp](controllers/search.hpp) | `HttpController` | 1 | 是 | 地理空间搜索 |
| `LocationController` | [location.hpp](controllers/location.hpp) | `HttpController` | 4 | 是 | PostGIS 位置 |
| `MediaController` | [media_server.hpp](controllers/media_server.hpp) | `HttpController` | 5 | 是 | 预签名 URL |
| `NotificationWebSocket` | [notification_websocket.hpp](controllers/notification_websocket.hpp) | `WebSocketController` | 1 | 是 | 实时推送 |

`Authentication` 的“混合”认证状态反映了其设计：`/login`、`/register` 和 `/refresh` 不需要令牌（它们是签发令牌的端点），而 `/logout` 受 `AuthMiddleware` 保护，以识别要注销的会话所属的用户。
- 有两点观察至关重要。 
- 首先，每条路由都包含 `Options` ——这并非偶然。`CorsMiddleware` 本身并不处理预检 `OPTIONS` 请求；
- 相反，每条路由必须显式接受它们，以便 Drogon 的路由器能够匹配预检请求。
- 实际的 CORS 头注入发生在中间件的响应拦截器中（`CorsMiddleware.cc`）。
- 其次，`CorsMiddleware` 强制仅限环回访问 ——任何来自非环回 IP 的请求都会立即收到 404 响应。这是一个开发阶段的安全网，而非生产环境的 CORS 策略。

> `CorsMiddleware` 环回检查意味着在开发环境中，API 客户端必须通过 `127.0.0.1` 或 `::1` 进行连接。
> 来自局域网 IP（如 `192.168.x.x`）的请求将收到不带任何 CORS 头的 404 响应，这在浏览器中会表现为不透明的 CORS 错误——而不是明确的“需要环回”提示信息。
---

```mermaid
flowchart TB
    subgraph Client["客户端层"]
        FE["前端应用"]
    end

    subgraph Server["Drogon HTTP 服务器 :5555"]
        direction TB
        CORS["CorsMiddleware 跨域"]
        AUTH["AuthMiddleware 认证"]
        WS_AUTH["WebSocketAuthMiddleware WebSocket认证"]

        subgraph Controllers["API 控制器"]
            AUTH_C["Authentication 认证控制器"]
            COMM["Community 社区控制器"]
            OFFERS["Offers 优惠控制器"]
            CHATS["Chats 聊天控制器"]
            ORDERS["Orders 订单控制器"]
            SEARCH["Search 搜索控制器"]
            LOC["Location 位置控制器"]
            MEDIA["Media 媒体控制器"]
            DASH["Dashboard 仪表盘控制器"]
            USERS["Users 用户控制器"]
        end
    end

    subgraph Services["服务层"]
        SM["ServiceManager 服务管理器"]
        PM["PubManager<br/>(ZeroMQ PUB) 发布管理器"]
        SUBM["SubManager<br/>(ZeroMQ SUB) 订阅管理器"]
        CM["ConnectionManager 连接管理器"]
        S3["S3Service<br/>(Minio) S3 服务"]
    end

    subgraph Infra["基础设施"]
        PG["PostgreSQL<br/>+ PostGIS 5432 <br/>数据库"]
        ZMQ["ZeroMQ<br/>进程内消息队列"]
        MINIO["Minio<br/>S3 API9000 <br/>Web控制台9001 <br/> 对象存储服务"]
    end

    FE -->|"HTTP / WS"| CORS --> AUTH --> Controllers
    FE -->|"WS /ws/notifications"| WS_AUTH --> NWSC["NotificationWebSocket"]
    Controllers --> SM
    SM --> PM & SUBM & CM & S3
    PM --> ZMQ --> SUBM
    SUBM --> CM -->|"广播"| NWSC
    Controllers --> PG
    S3 --> MINIO
```

---
每个 HTTP 请求在到达控制器之前，都会经过一条**中间件链**。Drogon 支持同步（HttpMiddleware<T>）和基于协程（HttpCoroMiddleware<T>）的中间件，本项目两者兼用：
```mermaid
flowchart TD
%% 1. 定义样式类
    classDef start fill:#e3f2fd,stroke:#1565c0,color:#0d47a1;
    classDef process fill:#fff9c4,stroke:#fbc02d,color:#f57f17;
    classDef success fill:#c8e6c9,stroke:#2e7d32,color:#1b5e20;
    classDef error fill:#ffccbc,stroke:#d84315,color:#bf360c;

%% 2. 节点定义 (纯净写法，不带样式)
    Start("🔵 传入请求")
    CORS["CorsMiddleware<br/>(同步处理)"]
    Reject("❌ 404 拒绝")
    OptionsPass("✅ 200 OPTIONS 放行")
    Auth["AuthMiddleware<br/>(协程/异步)"]
    Controller("⚙️ 进入控制器")
    Err("🚫 401 未授权")
    IsOptions{"请求方法<br/>是 OPTIONS?"}

%% 3. 应用样式 (单独指定)
    class Start start
    class CORS,Auth process
    class Reject,Err error
    class OptionsPass,Controller success

%% 4. 流程连接
    Start --> CORS
    CORS -- "非回环源/非法源" --> Reject
    CORS -- "回环源/合法源" --> IsOptions
    IsOptions -- "是" --> OptionsPass
    IsOptions -- "否" --> Auth
    Auth -- "有效 JWT" --> Controller
    Auth -- "缺失/无效 JWT" --> Err
```
---
## 控制器执行流
每个经过认证的请求从到达至响应，都会经历一个五阶段管道。下图追踪了典型认证端点（如创建报价）的完整生命周期。

```mermaid
sequenceDiagram
    participant C as Client<br/>客户端
    participant CORS as CorsMiddleware<br/>跨域中间件
    participant AUTH as AuthMiddleware<br/>认证中间件
    participant CTRL as Offers Controller<br/>报价控制器
    participant DB as PostgreSQL<br/>数据库

    C->>CORS: POST /api/v1/posts/{id}/offers
    CORS->>CORS: Check loopback IP<br/>检查本地回环IP
    CORS->>AUTH: Forward request<br/>转发请求
    AUTH->>AUTH: Extract Bearer token<br/>提取Bearer令牌
    AUTH->>AUTH: Verify JWT signature & expiry<br/>验证JWT签名与过期时间
    AUTH->>AUTH: Insert current_user_id attribute<br/>存入当前用户ID到请求属性
    AUTH->>CTRL: Forward request<br/>转发请求
    CTRL->>CTRL: Parse JSON body (strict_read_json)<br/>严格解析JSON请求体
    CTRL->>DB: BEGIN TRANSACTION<br/>开启事务
    CTRL->>DB: INSERT INTO offers (...)<br/>插入offer数据
    CTRL->>CTRL: quick_process_media_attachments()<br/>快速处理媒体附件
    DB-->>CTRL: Transaction result<br/>返回事务结果
    CTRL->>C: 200 JSON response<br/>返回200 JSON响应
```

## 实时通知架构

```mermaid
sequenceDiagram
    autonumber
    actor C as 控制器线程
    participant PM as PubManager<br/>(ZMQ PUB)
    participant SM as SubManager<br/>(ZMQ SUB)
    participant CM as ConnectionManager
    participant WS as WebSocket 客户端

    Note over C,WS: 📢 消息广播流程

    C->>PM: publish("offer_created:42", payload)
    
    PM->>SM: inproc://pubsub<br/>(ZMQ 内部通信)
    
    SM->>CM: broadcast("offer_created:42", payload)
    
    rect rgb(10, 200, 10)
        Note right of CM: 持久化与分发
        CM->>CM: store_notification_in_db<br/>(conn_id, payload)
        CM->>WS: send(payload)<br/>[所有已订阅的连接]
    end
```

## JWT 身份验证流程

```mermaid
graph TB
    %% --- 样式定义 ---
    classDef client fill:#e3f2fd,stroke:#1565c0,color:#0d47a1;
    classDef middleware fill:#fff9c4,stroke:#fbc02d,color:#f57f17;
    classDef controller fill:#c8e6c9,stroke:#2e7d32,color:#1b5e20;
    classDef crypto fill:#f3e5f5,stroke:#7b1fa2,color:#4a148c;
    classDef db fill:#eeeeee,stroke:#616161,color:#212121,stroke-dasharray: 5 5;

    %% --- 节点定义 ---
    subgraph ClientLayer["📱 客户端层"]
        REQ["HTTP 请求<br/>POST /api/v1/auth/*"]:::client
    end

    subgraph Framework["🚀 Drogon 框架层"]
        CORS["CorsMiddleware"]:::middleware
        AUTH["AuthMiddleware<br/>(JWT 验证)"]:::middleware
        CTRL["认证控制器<br/>(AuthController)"]:::controller
    end

    subgraph Security["🔐 加密与安全层"]
        A2["Argon2id<br/>(密码哈希)"]:::crypto
        JWT_GEN["jwt-cpp HS256<br/>(Token 生成)"]:::crypto
        JWT_VER["jwt-cpp HS256<br/>(Token 验证)"]:::crypto
        RNG["加密级别RNG<br/>(刷新 Token 生成)"]:::crypto
    end

    subgraph Storage["💾 数据存储层"]
        USERS[("users 表<br/>密码哈希存储)")]:::db
        SESSIONS[("user_sessions 表<br/>刷新 Token 存储)")]:::db
    end

    %% --- 连接关系 ---
    REQ --> CORS
    CORS --> AUTH
    AUTH -- "有效 Token" --> CTRL
    AUTH -- "无效/缺失" --> ERR401["401 Unauthorized"]:::client

    %% 控制器调用
    CTRL -- "1. 注册: 哈希" --> A2
    CTRL -- "2. 登录: 生成" --> JWT_GEN
    CTRL -- "3. 刷新: 随机串" --> RNG
    CTRL -- "验证时调用" --> JWT_VER

    %% 数据持久化
    CTRL -- "用户查询"--> USERS
    CTRL -- "会话持久化"--> SESSIONS
    JWT_VER -- "读取密钥" --> Config["JWT_SECRET<br/>(配置)"]:::crypto
```

## 注册流程

```mermaid
flowchart LR
    %% --- 样式定义 ---
    classDef success fill:#d4edda,stroke:#c3e6cb,color:#155724;
    classDef error fill:#f8d7da,stroke:#f5c6cb,color:#721c24;
    classDef process fill:#cce5ff,stroke:#b8daff,color:#004085;
    classDef decision fill:#fff3cd,stroke:#ffeeba,color:#856404;

    %% --- 1. 请求入口 ---
    subgraph Request["📥 请求处理层"]
        A["POST /api/v1/auth/register<br/>{username, email, password}"]:::process
    end

    %% --- 2. 业务逻辑 ---
    subgraph Logic["⚙️ 业务逻辑层"]
        direction TB
        B{"解析并验证<br/>JSON 请求体"}:::decision
        C["使用 Argon2id<br/>哈希密码"]:::process
        D{"检查唯一性<br/>用户名 OR 电子邮件"}:::decision
        F["生成 JWT +<br/>刷新 Token"]:::process
    end

    %% --- 3. 数据持久化 ---
    subgraph Persistence["💾 数据持久化层"]
        direction TB
        E["INSERT INTO users<br/>RETURNING id"]:::process
        G["INSERT INTO user_sessions<br/>(持久化会话)"]:::process
    end

    %% --- 4. 响应结果 ---
    subgraph Response["📤 响应结果"]
        direction TB
        R200["✅ 200 OK<br/>{status, token, refresh_token,<br/>user_id, username}"]:::success
        R400["❌ 400 Bad Request<br/>字段缺失或格式错误"]:::error
        R409["⚠️ 409 Conflict<br/>用户名或邮箱已存在"]:::error
        R500A["💥 500 Error<br/>密码哈希失败"]:::error
        R500B["💥 500 Error<br/>数据库写入失败"]:::error
        R500C["⚠️ 500 Error<br/>注册成功但登录失败"]:::error
    end

    %% --- 流程连接 ---
    A --> B
    B -- "无效数据" --> R400
    B -- "有效" --> C
    
    C -- "哈希失败" --> R500A
    C -- "成功" --> D
    
    D -- "发现重复" --> R409
    D -- "唯一" --> E
    
    E -- "数据库错误" --> R500B
    E -- "成功" --> F
    
    F --> G
    G -- "会话存储失败" --> R500C
    G -- "成功" --> R200
```

## Token 刷新流程

```mermaid
flowchart LR
    %% --- 样式定义 ---
    classDef success fill:#d4edda,stroke:#c3e6cb,color:#155724;
    classDef error fill:#f8d7da,stroke:#f5c6cb,color:#721c24;
    classDef process fill:#cce5ff,stroke:#b8daff,color:#004085;
    classDef decision fill:#fff3cd,stroke:#ffeeba,color:#856404;

    %% --- 1. 请求入口 ---
    subgraph Request["📥 请求处理层"]
        A["POST /api/v1/auth/refresh<br/>{refresh_token}"]:::process
    end

    %% --- 2. 业务逻辑 ---
    subgraph Logic["⚙️ 业务逻辑与验证"]
        direction TB
        B{"解析请求体"}:::decision
        D{"查询 user_sessions<br/>(WHERE refresh_token = ?)"}:::process
        F{"解析 expires_at<br/>(PostgreSQL 时间戳)"}:::process
        H{"expires_at < now() ?"}:::decision
        J["生成新 JWT +<br/>新 Refresh Token"]:::process
    end

    %% --- 3. 数据持久化 ---
    subgraph Persistence["💾 数据持久化层"]
        K["UPDATE user_sessions<br/>SET token, refresh_token,<br/>expires_at"]:::process
    end

    %% --- 4. 响应结果 ---
    subgraph Response["📤 响应结果"]
        direction TB
        L["✅ 200 OK<br/>新凭据"]:::success
        C["❌ 400 Bad Request<br/>请求体缺失"]:::error
        E["🚫 401 Unauthorized<br/>'Invalid refresh token'"]:::error
        G["💥 500 Internal Server Error<br/>时间戳解析失败"]:::error
        I["⏳ 401 Unauthorized<br/>'Refresh token has expired'"]:::error
    end

    %% --- 流程连接 ---
    A --> B
    B -- "缺失" --> C
    B -- "有效" --> D
    
    D -- "无记录" --> E
    D -- "找到" --> F
    
    F -- "解析错误" --> G
    F -- "已解析" --> H
    
    H -- "已过期" --> I
    H -- "有效 (未过期)" --> J
    
    J --> K
    K --> L
```

## 中间件过滤器

```mermaid
flowchart TB
    %% --- 样式定义 ---
    classDef client fill:#e3f2fd,stroke:#1565c0,color:#0d47a1;
    classDef framework fill:#fff9c4,stroke:#fbc02d,color:#f57f17;
    classDef filter fill:#e8f5e9,stroke:#2e7d32,color:#1b5e20;
    classDef controller fill:#f3e5f5,stroke:#7b1fa2,color:#4a148c;
    classDef error fill:#ffccbc,stroke:#d84315,color:#bf360c;

    %% --- 1. 客户端 ---
    subgraph ClientLayer["📱 客户端层"]
        B["Browser / WS Client"]:::client
    end

    %% --- 2. 框架入口 ---
    subgraph FrameworkLayer["🚀 Drogon 框架入口"]
        R["HTTP/WS Request<br/>(统一入口)"]:::framework
    end

    %% --- 3. 过滤器管道 ---
    subgraph FilterPipeline["🛡️ 过滤器管道 (Filters)"]
        direction TB
        CM["CorsMiddleware<br/>(Loopback Guard + CORS Headers)"]:::filter
        
        subgraph AuthGroup["认证分支"]
            direction LR
            AM["AuthMiddleware<br/>(HTTP: JWT Header)"]:::filter
            WM["WebSocketAuthMiddleware<br/>(WS: JWT Query/Header)"]:::filter
        end
    end

    %% --- 4. 控制器层 ---
    subgraph ControllerLayer["⚙️ 控制器层 (Controllers)"]
        direction TB
        HC["HttpController<br/>(Community, Offers, Chats...)"]:::controller
        WC["WebSocketController<br/>(NotificationWebSocket)"]:::controller
    end

    %% --- 5. 响应结果 ---
    NF["404 Not Found<br/>(非回环源拒绝)"]:::error
    U1["401 Unauthorized<br/>(HTTP: 无效 JWT)"]:::error
    U2["401 Unauthorized<br/>(WS: 无效 JWT)"]:::error

    %% --- 流程连接 ---
    %% 入口
    B ==>|"HTTP 或 WS 连接"| R
    R --> CM

    %% CORS 检查
    CM -- "非回环源 ✗" --> NF
    
    %% 分流逻辑
    CM -- "回环源 ✓ (HTTP)" --> AM
    CM -- "回环源 ✓ (WS)" --> WM

    %% 认证与执行
    AM -- "有效 JWT" --> HC
    AM -- "无效/缺失 JWT" --> U1

    WM -- "有效 JWT" --> WC
    WM -- "无效/缺失 JWT" --> U2
```

## 令牌提取与验证

```mermaid
flowchart TD
    %% --- 样式定义 ---
    classDef success fill:#d4edda,stroke:#c3e6cb,color:#155724;
    classDef error fill:#f8d7da,stroke:#f5c6cb,color:#721c24;
    classDef process fill:#cce5ff,stroke:#b8daff,color:#004085;
    classDef decision fill:#fff3cd,stroke:#ffeeba,color:#856404;

    %% --- 1. 请求预处理 ---
    subgraph Preprocessing["🔍 请求预处理"]
        A["提取 Authorization 头"]:::process
    end

    %% --- 2. 令牌解析与验证 ---
    subgraph Validation["🔐 令牌解析与验证"]
        B{"头为空 或<br/>无 'Bearer ' 前缀？"}:::decision
        C["使用 jsoncpp traits<br/>解码 JWT"]:::process
        D["验证 HS256 签名<br/>(JWT_SECRET + 'buyer-app')"]:::process
    end

    %% --- 3. 用户身份提取 ---
    subgraph Extraction["👤 用户身份提取"]
        direction TB
        F{"载荷声明 'user_id'<br/>类型？"}:::decision
        G1["user_id = to_string<br/>(claim.as_number())"]:::process
        G2["user_id = claim.as_string()"]:::process
    end

    %% --- 4. 上下文注入 ---
    subgraph Injection["📥 上下文注入"]
        H["req->attributes->insert<br/>'current_user_id', user_id"]:::process
    end

    %% --- 5. 响应结果 ---
    subgraph Response["📤 响应结果"]
        direction TB
        I["✅ co_await next<br/>→ 控制器"]:::success
        E1["❌ 401: 未提供有效令牌"]:::error
        E2["❌ 401: 无效令牌<br/>(user_id 类型错误)"]:::error
        E3["❌ 401: 无效令牌<br/>(签名验证失败)"]:::error
    end

    %% --- 流程连接 ---
    A --> B
    B -- "是" --> E1
    B -- "否" --> C
    
    C --> D
    D -- "验证失败" --> E3
    D -- "验证成功" --> F
    
    F -- "integer/number" --> G1
    F -- "string" --> G2
    F -- "其他" --> E2
    
    G1 --> H
    G2 --> H
    
    H --> I
```