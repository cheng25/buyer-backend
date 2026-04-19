-- Tables from 001_create_tables.sql
-- 用户表
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL, -- 用户名
    email VARCHAR(100) UNIQUE NOT NULL, -- 邮箱地址
    password_hash VARCHAR(255) NOT NULL, -- 密码哈希值
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 更新时间
);

COMMENT ON TABLE users IS '用户表';
COMMENT ON COLUMN users.id IS '用户ID';
COMMENT ON COLUMN users.username IS '用户名';
COMMENT ON COLUMN users.email IS '邮箱地址';
COMMENT ON COLUMN users.password_hash IS '密码哈希值';
COMMENT ON COLUMN users.created_at IS '创建时间';
COMMENT ON COLUMN users.updated_at IS '更新时间';

-- 用户会话表
CREATE TABLE user_sessions (
    id SERIAL PRIMARY KEY,
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    token VARCHAR(255) UNIQUE NOT NULL, -- 访问令牌
    refresh_token VARCHAR(255) UNIQUE, -- 刷新令牌
    device_info TEXT, -- 设备信息
    ip_address VARCHAR(45), -- IP地址
    expires_at TIMESTAMP NOT NULL, -- 过期时间
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE user_sessions IS '用户会话表';
COMMENT ON COLUMN user_sessions.id IS '会话ID';
COMMENT ON COLUMN user_sessions.user_id IS '用户ID';
COMMENT ON COLUMN user_sessions.token IS '访问令牌';
COMMENT ON COLUMN user_sessions.refresh_token IS '刷新令牌';
COMMENT ON COLUMN user_sessions.device_info IS '设备信息';
COMMENT ON COLUMN user_sessions.ip_address IS 'IP地址';
COMMENT ON COLUMN user_sessions.expires_at IS '过期时间';
COMMENT ON COLUMN user_sessions.created_at IS '创建时间';

        -- 订单表
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    user_id INT REFERENCES users (id), -- 用户ID
    status VARCHAR(50) NOT NULL, -- 订单状态
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE orders IS '订单表';
COMMENT ON COLUMN orders.id IS '订单ID';
COMMENT ON COLUMN orders.user_id IS '用户ID';
COMMENT ON COLUMN orders.status IS '订单状态';
COMMENT ON COLUMN orders.created_at IS '创建时间';

        -- 媒体文件表
CREATE TABLE media (
    id SERIAL PRIMARY KEY,
    uploader_id INT REFERENCES users (id) ON DELETE SET NULL, -- 上传者ID
    storage_key VARCHAR(300) NOT NULL UNIQUE, -- 存储键存储键（uploads/uuid_filename.extension）
    file_name VARCHAR(255), -- 文件名
    mime_type VARCHAR(127), -- MIME类型
    size BIGINT, -- 文件大小
    metadata JSONB DEFAULT '{}'::jsonb, -- 元数据
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE media IS '媒体文件表';
COMMENT ON COLUMN media.id IS '媒体ID';
COMMENT ON COLUMN media.uploader_id IS '上传者ID';
COMMENT ON COLUMN media.storage_key IS '存储键（uploads/uuid_filename.extension）';
COMMENT ON COLUMN media.file_name IS '文件名';
COMMENT ON COLUMN media.mime_type IS 'MIME类型';
COMMENT ON COLUMN media.size IS '文件大小（字节）';
COMMENT ON COLUMN media.metadata IS '元数据';
COMMENT ON COLUMN media.created_at IS '创建时间';

    -- 帖子表
CREATE TABLE posts (
    id SERIAL PRIMARY KEY,
    user_id INT REFERENCES users (id), -- 用户ID
    content TEXT NOT NULL, -- 内容
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    -- Fields from 002_enhance_posts.sql
    tags TEXT[] DEFAULT '{}', -- 标签数组
    location VARCHAR(255), -- 位置
    is_product_request BOOLEAN DEFAULT FALSE, -- 是否为产品请求
    request_status VARCHAR(50) DEFAULT 'open', -- 请求状态
    price_range VARCHAR(100) -- 价格范围
);

COMMENT ON TABLE posts IS '帖子表';
COMMENT ON COLUMN posts.id IS '帖子ID';
COMMENT ON COLUMN posts.user_id IS '用户ID';
COMMENT ON COLUMN posts.content IS '帖子内容';
COMMENT ON COLUMN posts.created_at IS '创建时间';
COMMENT ON COLUMN posts.tags IS '标签数组';
COMMENT ON COLUMN posts.location IS '位置';
COMMENT ON COLUMN posts.is_product_request IS '是否为产品请求';
COMMENT ON COLUMN posts.request_status IS '请求状态';
COMMENT ON COLUMN posts.price_range IS '价格范围';


CREATE TABLE post_media (
    post_id INT REFERENCES posts(id) ON DELETE CASCADE, -- 帖子ID
    media_id INT REFERENCES media(id) ON DELETE CASCADE, -- 媒体ID
    PRIMARY KEY (post_id, media_id)
);

COMMENT ON TABLE post_media IS '帖子媒体关联表';
COMMENT ON COLUMN post_media.post_id IS '帖子ID';
COMMENT ON COLUMN post_media.media_id IS '媒体ID';

        -- 聊天会话表
CREATE TABLE conversations (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100), -- 会话名称
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE conversations IS '聊天会话表';
COMMENT ON COLUMN conversations.id IS '会话ID';
COMMENT ON COLUMN conversations.name IS '会话名称';
COMMENT ON COLUMN conversations.created_at IS '创建时间';

        -- 会话参与者表
CREATE TABLE conversation_participants (
    conversation_id INT REFERENCES conversations (id) ON DELETE CASCADE, -- 会话ID
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    PRIMARY KEY (conversation_id, user_id)
);

COMMENT ON TABLE conversation_participants IS '会话参与者表';
COMMENT ON COLUMN conversation_participants.conversation_id IS '会话ID';
COMMENT ON COLUMN conversation_participants.user_id IS '用户ID';

        -- 消息表
CREATE TABLE messages (
    id SERIAL PRIMARY KEY,
    conversation_id INT REFERENCES conversations (id) ON DELETE CASCADE, -- 会话ID
    sender_id INT REFERENCES users (id) ON DELETE CASCADE, -- 发送者ID
    content TEXT NOT NULL, -- 消息内容
    message_type VARCHAR(20) DEFAULT 'text', -- 'text', 'media', 'mixed'    
    is_read BOOLEAN DEFAULT FALSE, -- 是否已读
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    context_type VARCHAR(50), -- 上下文类型
    context_id INT, -- 上下文ID
    metadata JSONB DEFAULT '{}'::jsonb -- 元数据
);

COMMENT ON TABLE messages IS '消息表';
COMMENT ON COLUMN messages.id IS '消息ID';
COMMENT ON COLUMN messages.conversation_id IS '会话ID';
COMMENT ON COLUMN messages.sender_id IS '发送者ID';
COMMENT ON COLUMN messages.content IS '消息内容';
COMMENT ON COLUMN messages.message_type IS '消息类型（text:文本, media:媒体, mixed:混合）';
COMMENT ON COLUMN messages.is_read IS '是否已读';
COMMENT ON COLUMN messages.created_at IS '创建时间';
COMMENT ON COLUMN messages.context_type IS '上下文类型';
COMMENT ON COLUMN messages.context_id IS '上下文ID';
COMMENT ON COLUMN messages.metadata IS '元数据';

        -- 消息媒体关联表
CREATE TABLE message_media (
    message_id INT REFERENCES messages(id) ON DELETE CASCADE, -- 消息ID
    media_id INT REFERENCES media(id) ON DELETE CASCADE, -- 媒体ID
    PRIMARY KEY (message_id, media_id)
);

COMMENT ON TABLE message_media IS '消息媒体关联表';
COMMENT ON COLUMN message_media.message_id IS '消息ID';
COMMENT ON COLUMN message_media.media_id IS '媒体ID';

-- Table from 002_enhance_posts.sql
        -- 帖子订阅表
CREATE TABLE post_subscriptions (
    id SERIAL PRIMARY KEY,
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    post_id INT REFERENCES posts (id) ON DELETE CASCADE, -- 帖子ID
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    UNIQUE (user_id, post_id)
);

COMMENT ON TABLE post_subscriptions IS '帖子订阅表';
COMMENT ON COLUMN post_subscriptions.id IS '订阅ID';
COMMENT ON COLUMN post_subscriptions.user_id IS '用户ID';
COMMENT ON COLUMN post_subscriptions.post_id IS '帖子ID';
COMMENT ON COLUMN post_subscriptions.created_at IS '创建时间';

-- Tables from 003_create_offers_table.sql
        -- 报价表
CREATE TABLE offers (
    id SERIAL PRIMARY KEY,
    post_id INT REFERENCES posts (id) ON DELETE CASCADE, -- 帖子ID
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    title VARCHAR(255) NOT NULL, -- 标题
    description TEXT NOT NULL, -- 描述
    price DECIMAL(10, 2), -- 价格
    is_public BOOLEAN DEFAULT TRUE, -- 是否公开
    status VARCHAR(50) DEFAULT 'pending', -- 状态
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 更新时间
    -- Fields from 004_negotiation_and_product_proof.sql
    negotiation_status VARCHAR(50) DEFAULT 'none', -- 议价状态
    original_price DECIMAL(10, 2) NOT NULL -- 原始价格
);

COMMENT ON TABLE offers IS '报价表';
COMMENT ON COLUMN offers.id IS '报价ID';
COMMENT ON COLUMN offers.post_id IS '帖子ID';
COMMENT ON COLUMN offers.user_id IS '用户ID';
COMMENT ON COLUMN offers.title IS '报价标题';
COMMENT ON COLUMN offers.description IS '报价描述';
COMMENT ON COLUMN offers.price IS '价格';
COMMENT ON COLUMN offers.is_public IS '是否公开';
COMMENT ON COLUMN offers.status IS '报价状态';
COMMENT ON COLUMN offers.created_at IS '创建时间';
COMMENT ON COLUMN offers.updated_at IS '更新时间';
COMMENT ON COLUMN offers.negotiation_status IS '议价状态';
COMMENT ON COLUMN offers.original_price IS '原始价格';

        -- 报价媒体关联表
CREATE TABLE offer_media (
    offer_id INT REFERENCES offers(id) ON DELETE CASCADE, -- 报价ID
    media_id INT REFERENCES media(id) ON DELETE CASCADE, -- 媒体ID
    PRIMARY KEY (offer_id, media_id)
);

COMMENT ON TABLE offer_media IS '报价媒体关联表';
COMMENT ON COLUMN offer_media.offer_id IS '报价ID';
COMMENT ON COLUMN offer_media.media_id IS '媒体ID';

        -- 报价通知表
CREATE TABLE offer_notifications (
    id SERIAL PRIMARY KEY,
    offer_id INT REFERENCES offers (id) ON DELETE CASCADE, -- 报价ID
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    is_read BOOLEAN DEFAULT FALSE, -- 是否已读
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE offer_notifications IS '报价通知表';
COMMENT ON COLUMN offer_notifications.id IS '通知ID';
COMMENT ON COLUMN offer_notifications.offer_id IS '报价ID';
COMMENT ON COLUMN offer_notifications.user_id IS '用户ID';
COMMENT ON COLUMN offer_notifications.is_read IS '是否已读';
COMMENT ON COLUMN offer_notifications.created_at IS '创建时间';

-- Tables from 004_negotiation_and_product_proof.sql
        -- 产品证明表
CREATE TABLE product_proofs (
    id SERIAL PRIMARY KEY,
    offer_id INT REFERENCES offers (id) ON DELETE CASCADE, -- 报价ID
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    image_url TEXT NOT NULL, -- 图片URL
    description TEXT, -- 描述
    status VARCHAR(50) DEFAULT 'pending', -- 状态
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE product_proofs IS '产品证明表';
COMMENT ON COLUMN product_proofs.id IS '证明ID';
COMMENT ON COLUMN product_proofs.offer_id IS '报价ID';
COMMENT ON COLUMN product_proofs.user_id IS '用户ID';
COMMENT ON COLUMN product_proofs.image_url IS '图片URL';
COMMENT ON COLUMN product_proofs.description IS '描述';
COMMENT ON COLUMN product_proofs.status IS '状态';
COMMENT ON COLUMN product_proofs.created_at IS '创建时间';

        -- 价格议表
CREATE TABLE price_negotiations (
    id SERIAL PRIMARY KEY,
    offer_id INT REFERENCES offers (id) ON DELETE CASCADE, -- 报价ID
    user_id INT REFERENCES users (id) ON DELETE CASCADE, -- 用户ID
    proposed_price DECIMAL(10, 2) NOT NULL, -- 提议价格
    status VARCHAR(50) DEFAULT 'pending', -- 状态
    message TEXT, -- 消息
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 更新时间
);

COMMENT ON TABLE price_negotiations IS '价格议体表';
COMMENT ON COLUMN price_negotiations.id IS '议价ID';
COMMENT ON COLUMN price_negotiations.offer_id IS '报价ID';
COMMENT ON COLUMN price_negotiations.user_id IS '用户ID';
COMMENT ON COLUMN price_negotiations.proposed_price IS '提议价格';
COMMENT ON COLUMN price_negotiations.status IS '状态';
COMMENT ON COLUMN price_negotiations.message IS '消息';
COMMENT ON COLUMN price_negotiations.created_at IS '创建时间';
COMMENT ON COLUMN price_negotiations.updated_at IS '更新时间';

        -- 托管交易表
CREATE TABLE escrow_transactions (
    id SERIAL PRIMARY KEY,
    offer_id INT REFERENCES offers (id) ON DELETE CASCADE, -- 报价ID
    buyer_id INT REFERENCES users (id) ON DELETE CASCADE, -- 买家ID
    seller_id INT REFERENCES users (id) ON DELETE CASCADE, -- 卖家ID
    amount DECIMAL(10, 2) NOT NULL, -- 金额
    status VARCHAR(50) DEFAULT 'pending', -- 状态
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 更新时间
);

COMMENT ON TABLE escrow_transactions IS '托管交易表';
COMMENT ON COLUMN escrow_transactions.id IS '交易ID';
COMMENT ON COLUMN escrow_transactions.offer_id IS '报价ID';
COMMENT ON COLUMN escrow_transactions.buyer_id IS '买家ID';
COMMENT ON COLUMN escrow_transactions.seller_id IS '卖家ID';
COMMENT ON COLUMN escrow_transactions.amount IS '金额';
COMMENT ON COLUMN escrow_transactions.status IS '状态';
COMMENT ON COLUMN escrow_transactions.created_at IS '创建时间';
COMMENT ON COLUMN escrow_transactions.updated_at IS '更新时间';

-- tables for Notifications
        -- 用户订阅表
CREATE TABLE user_subscriptions (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users (id), -- 用户ID
    subscription VARCHAR(255) NOT NULL, -- 订阅主题
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
    UNIQUE (user_id, subscription)
);

COMMENT ON TABLE user_subscriptions IS '用户订阅表';
COMMENT ON COLUMN user_subscriptions.id IS '订阅ID';
COMMENT ON COLUMN user_subscriptions.user_id IS '用户ID';
COMMENT ON COLUMN user_subscriptions.subscription IS '订阅主题';
COMMENT ON COLUMN user_subscriptions.created_at IS '创建时间';

        -- 通知类型枚举
CREATE TYPE notification_type AS ENUM (
    'post_created', -- 帖子创建
    'post_updated', -- 帖子更新

    'offer_created', -- 报价创建
    'offer_updated', -- 报价更新
    'offer_negotiated', -- 报价议价
    'offer_accepted', -- 报价接受
    'offer_rejected', -- 报价拒绝

    'chat_created', -- 聊天创建
    'message_sent', -- 消息发送
    'unknown' -- 未知
    -- Add more types as needed
);

COMMENT ON TYPE notification_type IS '通知类型枚举';

        -- 通知表
CREATE TABLE notifications (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users (id), -- 用户ID
    type notification_type NOT NULL, -- 通知类型
    message TEXT NOT NULL, -- 消息内容
    metadata JSONB, -- 元数据
    is_read BOOLEAN DEFAULT FALSE, -- 是否已读
    read_at TIMESTAMP, -- 阅读时间
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP -- 创建时间
);

COMMENT ON TABLE notifications IS '通知表';
COMMENT ON COLUMN notifications.id IS '通知ID';
COMMENT ON COLUMN notifications.user_id IS '用户ID';
COMMENT ON COLUMN notifications.type IS '通知类型';
COMMENT ON COLUMN notifications.message IS '消息内容';
COMMENT ON COLUMN notifications.metadata IS '元数据';
COMMENT ON COLUMN notifications.is_read IS '是否已读';
COMMENT ON COLUMN notifications.read_at IS '阅读时间';
COMMENT ON COLUMN notifications.created_at IS '创建时间';

CREATE EXTENSION IF NOT EXISTS postgis;

       -- 位置表
CREATE TABLE locations (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users (id), -- 用户ID
    latitude DOUBLE PRECISION NOT NULL, -- 纬度
    longitude DOUBLE PRECISION NOT NULL, -- 经度
    accuracy FLOAT, -- 精度
    device_id VARCHAR(255) NOT NULL, -- 设备ID
    cluster_id VARCHAR(64), -- 集群ID
    created_at TIMESTAMP DEFAULT NOW(), -- 创建时间
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 更新时间
    geom GEOGRAPHY (POINT, 4326), -- 地理位置
    UNIQUE (user_id)
);

COMMENT ON TABLE locations IS '位置表';
COMMENT ON COLUMN locations.id IS '位置ID';
COMMENT ON COLUMN locations.user_id IS '用户ID';
COMMENT ON COLUMN locations.latitude IS '纬度';
COMMENT ON COLUMN locations.longitude IS '经度';
COMMENT ON COLUMN locations.accuracy IS '精度';
COMMENT ON COLUMN locations.device_id IS '设备ID';
COMMENT ON COLUMN locations.cluster_id IS '集群ID';
COMMENT ON COLUMN locations.created_at IS '创建时间';
COMMENT ON COLUMN locations.updated_at IS '更新时间';
COMMENT ON COLUMN locations.geom IS '地理位置（PostGIS地理点）';

-- Indexes from 002_enhance_posts.sql
CREATE INDEX posts_tags_idx ON posts USING GIN (tags); -- 帖子标签索引（GIN索引用于数组查询）

-- Indexes from 003_create_offers_table.sql
CREATE INDEX offers_post_id_idx ON offers (post_id); -- 报价帖子ID索引

CREATE INDEX offers_user_id_idx ON offers (user_id); -- 报价用户ID索引

CREATE INDEX offer_notifications_user_id_idx ON offer_notifications (user_id); -- 报价通知用户ID索引

CREATE INDEX offer_notifications_is_read_idx ON offer_notifications (is_read); -- 报价通知已读状态索引

-- Indexes from 004_negotiation_and_product_proof.sql
CREATE INDEX product_proofs_offer_id_idx ON product_proofs (offer_id); -- 产品证明报价ID索引

CREATE INDEX price_negotiations_offer_id_idx ON price_negotiations (offer_id); -- 价格议价报价ID索引

CREATE INDEX escrow_transactions_offer_id_idx ON escrow_transactions (offer_id); -- 托管交易报价ID索引

CREATE INDEX messages_context_idx ON messages (context_type, context_id); -- 消息上下文索引

-- Fast retrieval of messages in a conversation, newest first
CREATE INDEX messages_conversation_created_at_idx ON messages(conversation_id, created_at DESC); -- 消息会话创建时间索引（按最新优先）

-- Fast lookup of unread messages in a conversation
CREATE INDEX messages_unread_idx ON messages(conversation_id, is_read) WHERE is_read = false; -- 消息未读索引（部分索引）

-- Queries by message_type
CREATE INDEX messages_type_idx ON messages(message_type); -- 消息类型索引

CREATE INDEX message_media_media_id_idx ON message_media(media_id); -- 消息媒体媒体ID索引

-- Fast lookup of media in posts and offers
CREATE INDEX post_media_media_id_idx ON post_media(media_id); -- 帖子媒体媒体ID索引

CREATE INDEX offer_media_media_id_idx ON offer_media(media_id); -- 报价媒体媒体ID索引

-- Indexes from notifications
CREATE INDEX user_subscriptions_user_id_idx ON user_subscriptions (user_id); -- 用户订阅用户ID索引

CREATE INDEX notifications_user_id_idx ON notifications (user_id); -- 通知用户ID索引

CREATE INDEX notifications_is_read_idx ON notifications (is_read); -- 通知已读状态索引

CREATE INDEX notifications_created_at_idx ON notifications (created_at); -- 通知创建时间索引

-- Create spatial index for fast queries
CREATE INDEX locations_geom_idx ON locations USING GIST (geom); -- 位置地理空间索引（GIST索引）

-- Create regular indexes for other queries
CREATE INDEX locations_cluster_idx ON locations (cluster_id); -- 位置集群ID索引

CREATE INDEX locations_device_idx ON locations (device_id); -- 位置设备ID索引
