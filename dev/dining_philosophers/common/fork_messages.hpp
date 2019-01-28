#pragma once

#include <so_5/all.hpp>

struct take_t
{
	const so_5::mbox_t m_who;
	std::size_t m_philosopher_index;
};

struct busy_t : public so_5::signal_t {};

struct taken_t : public so_5::signal_t {};

struct put_t : public so_5::signal_t {};

