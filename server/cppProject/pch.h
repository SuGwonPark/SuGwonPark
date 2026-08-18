#pragma once
#include <iostream>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <cstdint>
#include <queue>
#include <random>
#include <thread>
#include <atomic>
#include <vector>

#include <boost/asio.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;


#include <mysqlx/xdevapi.h>