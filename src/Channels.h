#ifndef CHANNELS_H
#define CHANNELS_H

#include <string>

void publish(const std::string& channel, const std::string& message);
std::string get(const std::string& channel);

void subscribe(const std::string& channel);
void unsubscribe(const std::string& channel);
size_t n_subscribers(const std::string& channel);

#endif //CHANNELS_H
