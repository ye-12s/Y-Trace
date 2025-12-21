#ifndef __VIRTUALBUS_HPP__
#define __VIRTUALBUS_HPP__

#include <cstdint>
#include <cstring>
namespace system
{
using topicId_t = uint32_t;
using vBusCallBack_t = void(*)(topicId_t topic, const void* data, std::size_t size);

template<std::size_t maxSubs>
class VirtualBus
{
public:
    ~VirtualBus() = default;

    template<typename obj, typename msg>
    bool subscribe(topicId_t topic, obj* o, void(obj::*method)(const msg &))
    {
        for (auto &s : subs_)
        {
            if (!s.active)
            {
                s.active = true;
                s.topic = topic;
                s.ctx = o;
                s.callback = &thunk<obj, msg, method>;
                return true;
            }
        }
        return false;
    }

    void publish(topicId_t topic, const void* data, std::size_t size)
    {
        for (auto &s : subs_)
        {
            if (s.active && s.topic == topic)
            {
                s.callback(topic, data, size);
            }
        }
    }


    bool register_topic(const char*name, topicId_t &out)
    {
        for (int i = 0; i < maxSubs; i++)
        {
            if (!subs_[i].active)
            {
                out = static_cast<topicId_t>(i + 1);
                // topic id registered, but not subscribed
                subs_[i] =
                {
                    .active = false,
                    .name = name,
                    .topic = out,
                    .ctx = nullptr,
                    .callback = nullptr
                };
                return true;
            }
        }
        return false;
    }

    topicId_t find_topic(const char *name)
    {
        for (const auto &s : subs_)
        {
            if (s.active && s.name != nullptr && std::strcmp(s.name, name) == 0)
            {
                return s.topic;
            }
        }
        return 0;
    }

private:
    struct slot
    {
        bool active{false};
        const char *name{nullptr};
        topicId_t topic{0};
        void *ctx{nullptr};
        vBusCallBack_t callback{nullptr};
    };

    slot subs_[maxSubs] {};

    template<typename obj, typename msg, void (obj::*method)(const msg &)>
    static void thunk(void *ctx, const void *data, std::size_t size)
    {
        (static_cast<obj*>(ctx)->*method)(*static_cast<const msg*>(data));
    }
};
}





#endif