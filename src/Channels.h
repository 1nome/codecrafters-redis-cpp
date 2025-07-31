#ifndef CHANNELS_H
#define CHANNELS_H

#include <set>

#include "Resp.h"
#include <string>

void publish(const std::string& channel, const std::string& message);
std::string get_message(const std::string& channel);

void subscribe(const std::string& channel);
void unsubscribe(const std::string& channel);
void unsubscribe(const std::set<std::string>& channels);
size_t n_subscribers(const std::string& channel);

const std::string message_bulk = bulk_string("message");
const std::string subscribe_bulk = bulk_string("subscribe");
const std::string unsubscribe_bulk = bulk_string("unsubscribe");

#endif //CHANNELS_H
