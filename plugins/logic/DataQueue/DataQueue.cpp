/**
 * @file DataQueue.cpp
 * @brief 数据队列工具 (对标 CKVision 数据队列, P0-9 补齐)
 *
 * 在流程内维护一条命名队列(先进先出), 跨工具/跨执行持久保存,
 * 用于缓存一批检测数据(如连续N帧测量值)供后续统计/判定。
 *
 * 操作模式:
 *   0 入队   把 value 追加到队尾
 *   1 出队   弹出队首值(空队列返回 defaultValue)
 *   2 窥视   取队首值不移除(空队列返回 defaultValue)
 *   3 清空   清空整条队列
 *   4 长度   仅返回队列长度(不动队列)
 *
 * 队列存储: 全局变量 "__DataQueue.<队列名>" (QVariantList), 线程安全。
 * 入队值来源: value 属性 > 上下文数据源 sourceDataKey > 全局变量同名键。
 * 输出: value(入队/出队/窥视的值) / queueLength / operation
 */
#include "DataQueue.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"
#include <QMutex>
#include <QVariantList>

namespace VisionInspector {

// 跨实例共享队列锁 (静态, 进程级)
static QMutex g_queueMutex;

PropertyDefList DataQueue::propertyDefs() const {
    return {
        PropertyDef::enumProp("operation", "操作",
                              {"入队", "出队", "窥视", "清空", "长度"}, 0),
        PropertyDef::stringProp("queueName", "队列名", "queue1"),
        PropertyDef::stringProp("value", "入队值", ""),
        PropertyDef::stringProp("sourceDataKey", "来源数据Key", ""),
        PropertyDef::stringProp("defaultValue", "空队列默认值", ""),
    };
}

static QString queueKey(const QString& name) {
    return QStringLiteral("__DataQueue.") + name;
}

bool DataQueue::execute(ToolContext& context) {
    const int op = propertyValue("operation").toInt();
    const QString name = propertyValue("queueName").toString();
    if (name.isEmpty()) {
        setResultData("error", "队列名为空");
        setStatus(ToolStatus::NG);
        return false;
    }

    GlobalVariables* gv = context.globalVariables();
    if (!gv) {
        setResultData("error", "无全局变量服务");
        setStatus(ToolStatus::NG);
        return false;
    }

    const QString key = queueKey(name);
    QVariant outValue;

    QMutexLocker locker(&g_queueMutex);

    // 读当前队列
    QVariantList queue = gv->get(key).toList();

    switch (op) {
    case 0: {  // 入队
        QVariant v = propertyValue("value");
        if (!v.isValid() || v.toString().isEmpty()) {
            const QString src = propertyValue("sourceDataKey").toString();
            if (!src.isEmpty() && context.hasData(src)) v = context.getData(src);
            else if (context.globalVariables()->has(name)) v = context.globalVariables()->get(name);
        }
        queue.append(v);
        outValue = v;
        break;
    }
    case 1:  // 出队
        if (!queue.isEmpty()) {
            outValue = queue.takeFirst();
        } else {
            outValue = propertyValue("defaultValue");
        }
        break;
    case 2:  // 窥视
        outValue = queue.isEmpty() ? QVariant(propertyValue("defaultValue")) : queue.first();
        break;
    case 3:  // 清空
        queue.clear();
        outValue = QVariant();
        break;
    case 4:  // 长度
        outValue = (int)queue.size();
        break;
    default:
        break;
    }

    // 写回 (长度操作不改变队列, 也写回无害)
    gv->set(key, queue);

    setResultData("value", outValue);
    setResultData("queueLength", (int)queue.size());
    setResultData("operation", op);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(DataQueue, "数据队列", VisionInspector::ToolCategory::Logic)
