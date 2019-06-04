/*
 * COPYRIGHT (C) 2017-2019, zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 */

#ifndef __ASIO2_UDP_SEND_COMPONENT_HPP__
#define __ASIO2_UDP_SEND_COMPONENT_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <future>

#include <asio2/base/selector.hpp>
#include <asio2/base/iopool.hpp>
#include <asio2/base/error.hpp>

#include <asio2/base/detail/util.hpp>

namespace asio2::detail
{
	template<class derived_t, bool isSession>
	class udp_send_cp
	{
	public:
		/**
		 * @constructor
		 */
		udp_send_cp(io_t & wio) : derive(static_cast<derived_t&>(*this)), wio_(wio) {}

		/**
		 * @destructor
		 */
		~udp_send_cp() = default;

	public:
		/**
		 * @function : Asynchronous send data,supporting multi data formats,see asio::buffer(...) in /asio/buffer.hpp
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class T>
		inline bool send(const std::string& host, const std::string& port, T&& data)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), d = std::forward<T>(data), host, port]()
					{
						derive._do_send(host, port, asio::buffer(d));
					}));
					return true;
				}

				return derive._do_send(host, port, asio::buffer(data));
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType * : send("abc");
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<CharT, char> ||
			std::is_same_v<CharT, wchar_t> ||
			std::is_same_v<CharT, char16_t> ||
			std::is_same_v<CharT, char32_t>, bool> send(const std::string& host, const std::string& port, const CharT * s)
		{
			return this->send(s, s ? Traits::length(s) : 0);
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline bool send(const std::string& host, const std::string& port, const CharT * s, const std::size_t count)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), s, count, host, port]()
					{
						derive._do_send(host, port, asio::buffer(s, count * sizeof(CharT)));
					}));
					return true;
				}

				return derive._do_send(host, port, asio::buffer(s, count * sizeof(CharT)));
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

		/**
		 * @function : Asynchronous send data,supporting multi data formats,see asio::buffer(...) in /asio/buffer.hpp
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = send(msg,asio::use_future); future.get(); it will cause deadlock,the future.get() will
		 * never return.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 */
		template<class T>
		inline std::future<std::pair<error_code, std::size_t>> send(
			const std::string& host, const std::string& port, T&& data, asio::use_future_t<> flag)
		{
			std::ignore = flag;
			std::promise<std::pair<error_code, std::size_t>> promise;
			std::future <std::pair<error_code, std::size_t>> future = promise.get_future();
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), d = std::forward<T>(data),
						pm = std::move(promise), host, port]() mutable
					{
						derive._do_send(host, port, asio::buffer(d), pm);
					}));
					return future;
				}

				derive._do_send(host, port, asio::buffer(data), promise);
			}
			catch (system_error & e)
			{
				set_last_error(e);
				promise.set_value(std::pair<error_code, std::size_t>(e.code(), 0));
			}
			catch (std::exception &)
			{
				set_last_error(asio::error::eof);
				promise.set_value(std::pair<error_code, std::size_t>(asio::error::eof, 0));
			}
			return future;
		}

		/**
		 * @function : Asynchronous send data
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = send(msg,asio::use_future); future.get(); it will cause deadlock,the future.get() will
		 * never return.
		 * PodType * : send("abc");
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<CharT, char> ||
			std::is_same_v<CharT, wchar_t> ||
			std::is_same_v<CharT, char16_t> ||
			std::is_same_v<CharT, char32_t>, std::future<std::pair<error_code, std::size_t>>>
			send(const std::string& host, const std::string& port, const CharT * s, asio::use_future_t<> flag)
		{
			return this->send(s, s ? Traits::length(s) : 0, std::move(flag));
		}

		/**
		 * @function : Asynchronous send data
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = send(msg,asio::use_future); future.get(); it will cause deadlock,the future.get() will
		 * never return.
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline std::future<std::pair<error_code, std::size_t>> send(const std::string& host, const std::string& port,
			const CharT * s, const std::size_t count, asio::use_future_t<> flag)
		{
			std::ignore = flag;
			std::promise<std::pair<error_code, std::size_t>> promise;
			std::future <std::pair<error_code, std::size_t>> future = promise.get_future();
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), s, count,
						pm = std::move(promise), host, port]() mutable
					{
						derive._do_send(host, port, asio::buffer(s, count * sizeof(CharT)), pm);
					}));
					return future;
				}

				derive._do_send(host, port, asio::buffer(s, count * sizeof(CharT)), promise);
			}
			catch (system_error & e)
			{
				set_last_error(e);
				promise.set_value(std::pair<error_code, std::size_t>(e.code(), 0));
			}
			catch (std::exception &)
			{
				set_last_error(asio::error::eof);
				promise.set_value(std::pair<error_code, std::size_t>(asio::error::eof, 0));
			}
			return future;
		}

		/**
		 * @function : Asynchronous send data,supporting multi data formats,see asio::buffer(...) in /asio/buffer.hpp
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class T, class Callback>
		inline bool send(const std::string& host, const std::string& port, T&& data, Callback&& fn)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), d = std::forward<T>(data),
						f = std::forward<Callback>(fn), host, port]()
					{
						derive._do_send(host, port, asio::buffer(d), f);
					}));
					return true;
				}

				return derive._do_send(host, port, asio::buffer(data), fn);
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType * : send("abc");
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class Callback, class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<CharT, char> ||
			std::is_same_v<CharT, wchar_t> ||
			std::is_same_v<CharT, char16_t> ||
			std::is_same_v<CharT, char32_t>, bool> send(
				const std::string& host, const std::string& port, const CharT * s, Callback&& fn)
		{
			return this->send(s, s ? Traits::length(s) : 0, std::forward<Callback>(fn));
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class Callback, class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline bool send(const std::string& host, const std::string& port, const CharT * s, const std::size_t count, Callback&& fn)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), s, count, f = std::forward<Callback>(fn), host, port]()
					{
						derive._do_send(host, port, asio::buffer(s, count * sizeof(CharT)), f);
					}));
					return true;
				}

				return derive._do_send(host, port, asio::buffer(s, count * sizeof(CharT)), fn);
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

	public:
		/**
		 * @function : Asynchronous send data,supporting multi data formats,see asio::buffer(...) in /asio/buffer.hpp
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class T>
		inline bool send(const asio::ip::udp::endpoint& endpoint, T&& data)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), d = std::forward<T>(data), endpoint]()
					{
						derive._do_send(endpoint, asio::buffer(d));
					}));
					return true;
				}

				return derive._do_send(endpoint, asio::buffer(data));
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType * : send("abc");
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<CharT, char> ||
			std::is_same_v<CharT, wchar_t> ||
			std::is_same_v<CharT, char16_t> ||
			std::is_same_v<CharT, char32_t>, bool> send(const asio::ip::udp::endpoint& endpoint, const CharT * s)
		{
			return this->send(s, s ? Traits::length(s) : 0);
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline bool send(const asio::ip::udp::endpoint& endpoint, const CharT * s, const std::size_t count)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), s, count, endpoint]()
					{
						derive._do_send(endpoint, asio::buffer(s, count * sizeof(CharT)));
					}));
					return true;
				}

				return derive._do_send(endpoint, asio::buffer(s, count * sizeof(CharT)));
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

		/**
		 * @function : Asynchronous send data,supporting multi data formats,see asio::buffer(...) in /asio/buffer.hpp
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = send(msg,asio::use_future); future.get(); it will cause deadlock,the future.get() will
		 * never return.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 */
		template<class T>
		inline std::future<std::pair<error_code, std::size_t>> send(
			const asio::ip::udp::endpoint& endpoint, T&& data, asio::use_future_t<> flag)
		{
			std::ignore = flag;
			std::promise<std::pair<error_code, std::size_t>> promise;
			std::future <std::pair<error_code, std::size_t>> future = promise.get_future();
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), d = std::forward<T>(data),
						pm = std::move(promise), endpoint]() mutable
					{
						derive._do_send(endpoint, asio::buffer(d), pm);
					}));
					return future;
				}

				derive._do_send(endpoint, asio::buffer(data), promise);
			}
			catch (system_error & e)
			{
				set_last_error(e);
				promise.set_value(std::pair<error_code, std::size_t>(e.code(), 0));
			}
			catch (std::exception &)
			{
				set_last_error(asio::error::eof);
				promise.set_value(std::pair<error_code, std::size_t>(asio::error::eof, 0));
			}
			return future;
		}

		/**
		 * @function : Asynchronous send data
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = send(msg,asio::use_future); future.get(); it will cause deadlock,the future.get() will
		 * never return.
		 * PodType * : send("abc");
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<CharT, char> ||
			std::is_same_v<CharT, wchar_t> ||
			std::is_same_v<CharT, char16_t> ||
			std::is_same_v<CharT, char32_t>, std::future<std::pair<error_code, std::size_t>>>
			send(const asio::ip::udp::endpoint& endpoint, const CharT * s, asio::use_future_t<> flag)
		{
			return this->send(s, s ? Traits::length(s) : 0, std::move(flag));
		}

		/**
		 * @function : Asynchronous send data
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * the pair.first save the send result error_code,the pair.second save the sent_bytes.
		 * note : Do not call this function in any listener callback function like this:
		 * auto future = send(msg,asio::use_future); future.get(); it will cause deadlock,the future.get() will
		 * never return.
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 */
		template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline std::future<std::pair<error_code, std::size_t>> send(const asio::ip::udp::endpoint& endpoint,
			const CharT * s, const std::size_t count, asio::use_future_t<> flag)
		{
			std::ignore = flag;
			std::promise<std::pair<error_code, std::size_t>> promise;
			std::future <std::pair<error_code, std::size_t>> future = promise.get_future();
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), s, count, pm = std::move(promise), endpoint]() mutable
					{
						derive._do_send(endpoint, asio::buffer(s, count * sizeof(CharT)), pm);
					}));
					return future;
				}

				derive._do_send(endpoint, asio::buffer(s, count * sizeof(CharT)), promise);
			}
			catch (system_error & e)
			{
				set_last_error(e);
				promise.set_value(std::pair<error_code, std::size_t>(e.code(), 0));
			}
			catch (std::exception &)
			{
				set_last_error(asio::error::eof);
				promise.set_value(std::pair<error_code, std::size_t>(asio::error::eof, 0));
			}
			return future;
		}

		/**
		 * @function : Asynchronous send data,supporting multi data formats,see asio::buffer(...) in /asio/buffer.hpp
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * use like this : std::string m; send(std::move(m)); can reducing memory allocation.
		 * PodType * : send("abc");
		 * PodType (&data)[N] : double m[10]; send(m);
		 * std::array<PodType, N> : std::array<int,10> m; send(m);
		 * std::vector<PodType, Allocator> : std::vector<float> m; send(m);
		 * std::basic_string<Elem, Traits, Allocator> : std::string m; send(m);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class T, class Callback>
		inline bool send(const asio::ip::udp::endpoint& endpoint, T&& data, Callback&& fn)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), d = std::forward<T>(data), f = std::forward<Callback>(fn), endpoint]()
					{
						derive._do_send(endpoint, asio::buffer(d), f);
					}));
					return true;
				}

				return derive._do_send(endpoint, asio::buffer(data), fn);
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType * : send("abc");
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class Callback, class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline typename std::enable_if_t<
			std::is_same_v<CharT, char> ||
			std::is_same_v<CharT, wchar_t> ||
			std::is_same_v<CharT, char16_t> ||
			std::is_same_v<CharT, char32_t>, bool> send(
				const asio::ip::udp::endpoint& endpoint, const CharT * s, Callback&& fn)
		{
			return this->send(s, s ? Traits::length(s) : 0, std::forward<Callback>(fn));
		}

		/**
		 * @function : Asynchronous send data
		 * You can call this function on the communication thread and anywhere,it's multi thread safed.
		 * Although the data object may be copied as necessary, ownership of the
		 * underlying memory blocks is retained by the caller, which must guarantee
		 * that they remain valid until the send operation is called.
		 * PodType (&data)[N] : double m[10]; send(m,5);
		 * We do not provide synchronous send function,because the synchronous send code is very simple,
		 * if you want use synchronous send data,you can do it like this (example):
		 * asio::write(session_ptr->stream(), asio::buffer(std::string("abc")));
		 */
		template<class Callback, class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
		inline bool send(const asio::ip::udp::endpoint& endpoint, const CharT * s, const std::size_t count, Callback&& fn)
		{
			// We must ensure that there is only one operation to send data
			// at the same time,otherwise may be cause crash.
			try
			{
				if (!derive.is_started())
					asio::detail::throw_error(asio::error::not_connected);

				if (!s)
					asio::detail::throw_error(asio::error::invalid_argument);

				// Make sure we run on the strand
				if (!this->wio_.strand().running_in_this_thread())
				{
					asio::post(this->wio_.strand(), make_allocator(derive.wallocator(),
						[this, p = this->_mkptr(), s, count, f = std::forward<Callback>(fn), endpoint]()
					{
						derive._do_send(endpoint, asio::buffer(s, count * sizeof(CharT)), f);
					}));
					return true;
				}

				return derive._do_send(endpoint, asio::buffer(s, count * sizeof(CharT)), fn);
			}
			catch (system_error & e) { set_last_error(e); }
			catch (std::exception &) { set_last_error(asio::error::eof); }
			return false;
		}

	protected:
		inline std::shared_ptr<derived_t> _mkptr()
		{
			if constexpr (isSession)
				return derive.shared_from_this();
			else
				return std::shared_ptr<derived_t>{};
		}

	protected:
		derived_t & derive;

		io_t      & wio_;
	};
}

#endif // !__ASIO2_UDP_SEND_COMPONENT_HPP__