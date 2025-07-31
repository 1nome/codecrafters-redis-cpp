#include "Channels.h"

#include <mutex>
#include <unordered_map>
#include <queue>

std::unordered_map<std::string, std::queue<std::string>> channels;
std::unordered_map<std::string, std::pair<size_t, size_t>> channel_subscribers;

std::mutex channels_lock;
std::mutex subscribers_lock;

void publish(const std::string& channel, const std::string& message)
{
    const std::lock_guard lock(channels_lock);
    if (!channels.contains(channel))
    {
        return;
    }
    channels[channel].push(message);
}

std::string get_message(const std::string& channel)
{
    const std::lock_guard lock(channels_lock);
    const std::lock_guard lock2(subscribers_lock);

    if (!channels.contains(channel))
    {
        return "";
    }
    auto& c = channels[channel];
    if (c.empty())
    {
        return "";
    }
    std::string s = c.front();
    auto& [total, remaining] = channel_subscribers[channel];
    remaining--;
    if (!remaining)
    {
        c.pop();
        remaining = total;
    }
    return s;
}

void subscribe(const std::string& channel)
{
    const std::lock_guard lock(subscribers_lock);
    const std::lock_guard lock2(channels_lock);
    auto& [total, remaining] = channel_subscribers[channel];
    remaining++;
    total++;
    channels[channel];
}

void unsubscribe(const std::string& channel)
{
    const std::lock_guard lock(subscribers_lock);
    const std::lock_guard lock2(channels_lock);
    auto& [total, remaining] = channel_subscribers[channel];
    if (remaining)
    {
        remaining--;
    }
    if (total)
    {
        total--;
    }
    if (!total)
    {
        channels.erase(channel);
    }
}

void unsubscribe(const std::set<std::string>& channels)
{
    for (const auto& channel : channels)
    {
        unsubscribe(channel);
    }
}

size_t n_subscribers(const std::string& channel)
{
    const std::lock_guard lock(subscribers_lock);
    return channel_subscribers[channel].first;
}
