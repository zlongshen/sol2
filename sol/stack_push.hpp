// The MIT License (MIT) 

// Copyright (c) 2013-2016 Rapptz, ThePhD and contributors

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SOL_STACK_PUSH_HPP
#define SOL_STACK_PUSH_HPP

#include "stack_core.hpp"
#include "raii.hpp"
#include "optional.hpp"
#include <memory>

namespace sol {
	namespace stack {
		template<typename T, typename>
		struct pusher {
			template <typename K, typename... Args>
			static int push_keyed(lua_State* L, K&& k, Args&&... args) {
				// Basically, we store all user-data like this:
				// If it's a movable/copyable value (no std::ref(x)), then we store the pointer to the new
				// data in the first sizeof(T*) bytes, and then however many bytes it takes to
				// do the actual object. Things that are std::ref or plain T* are stored as 
				// just the sizeof(T*), and nothing else.
				T** pointerpointer = static_cast<T**>(lua_newuserdata(L, sizeof(T*) + sizeof(T)));
				T*& referencereference = *pointerpointer;
				T* allocationtarget = reinterpret_cast<T*>(pointerpointer + 1);
				referencereference = allocationtarget;
				std::allocator<T> alloc{};
				alloc.construct(allocationtarget, std::forward<Args>(args)...);
				luaL_newmetatable(L, &k[0]);
				lua_setmetatable(L, -2);
				return 1;
			}

			template <typename... Args>
			static int push(lua_State* L, Args&&... args) {
				return push_keyed(L, usertype_traits<T>::metatable, std::forward<Args>(args)...);
			}
		};

		template<typename T>
		struct pusher<T*> {
			template <typename K>
			static int push_keyed(lua_State* L, K&& k, T* obj) {
				if (obj == nullptr)
					return stack::push(L, nil);
				T** pref = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));
				*pref = obj;
				luaL_newmetatable(L, &k[0]);
				lua_setmetatable(L, -2);
				return 1;
			}

			static int push(lua_State* L, T* obj) {
				return push_keyed(L, usertype_traits<meta::unqualified_t<T>*>::metatable, obj);
			}
		};

		template <>
		struct pusher<detail::as_reference_tag> {
			template <typename T>
			static int push(lua_State* L, T&& obj) {
				return stack::push(L, detail::ptr(obj));
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<is_unique_usertype<T>::value>> {
			typedef typename unique_usertype_traits<T>::type P;
			typedef typename unique_usertype_traits<T>::actual_type Real;

			template <typename Arg, meta::enable<std::is_base_of<Real, meta::unqualified_t<Arg>>> = meta::enabler>
			static int push(lua_State* L, Arg&& arg) {
				if (unique_usertype_traits<T>::is_null(arg))
					return stack::push(L, nil);
				return push_deep(L, std::forward<Arg>(arg));
			}

			template <typename Arg0, typename Arg1, typename... Args>
			static int push(lua_State* L, Arg0&& arg0, Arg0&& arg1, Args&&... args) {
				return push_deep(L, std::forward<Arg0>(arg0), std::forward<Arg1>(arg1), std::forward<Args>(args)...);
			}

			template <typename... Args>
			static int push_deep(lua_State* L, Args&&... args) {
				P** pref = static_cast<P**>(lua_newuserdata(L, sizeof(P*) + sizeof(detail::special_destruct_func) + sizeof(Real)));
				detail::special_destruct_func* fx = static_cast<detail::special_destruct_func*>(static_cast<void*>(pref + 1));
				Real* mem = static_cast<Real*>(static_cast<void*>(fx + 1));
				*fx = detail::special_destruct<P, Real>;
				detail::default_construct::construct(mem, std::forward<Args>(args)...);
				*pref = unique_usertype_traits<T>::get(*mem);
				if (luaL_newmetatable(L, &usertype_traits<detail::unique_usertype<P>>::metatable[0]) == 1) {
					set_field(L, "__gc", detail::unique_destruct<P>);
				}
				lua_setmetatable(L, -2);
				return 1;
			}
		};

		template<typename T>
		struct pusher<std::reference_wrapper<T>> {
			static int push(lua_State* L, const std::reference_wrapper<T>& t) {
				return stack::push(L, std::addressof(detail::deref(t.get())));
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<std::is_floating_point<T>::value>> {
			static int push(lua_State* L, const T& value) {
				lua_pushnumber(L, value);
				return 1;
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<meta::all<std::is_integral<T>, std::is_signed<T>>::value>> {
			static int push(lua_State* L, const T& value) {
				lua_pushinteger(L, static_cast<lua_Integer>(value));
				return 1;
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<std::is_enum<T>::value>> {
			static int push(lua_State* L, const T& value) {
				if (std::is_same<char, T>::value) {
					return stack::push(L, static_cast<int>(value));
				}
				return stack::push(L, static_cast<std::underlying_type_t<T>>(value));
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<meta::all<std::is_integral<T>, std::is_unsigned<T>>::value>> {
			static int push(lua_State* L, const T& value) {
				lua_pushinteger(L, static_cast<lua_Integer>(value));
				return 1;
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<meta::all<meta::has_begin_end<T>, meta::neg<meta::has_key_value_pair<T>>, meta::neg<meta::any<std::is_base_of<reference, T>, std::is_base_of<stack_reference, T>>>>::value>> {
			static int push(lua_State* L, const T& cont) {
				lua_createtable(L, static_cast<int>(cont.size()), 0);
				int tableindex = lua_gettop(L);
				unsigned index = 1;
				for (auto&& i : cont) {
					set_field(L, index++, i, tableindex);
				}
				return 1;
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<meta::all<meta::has_begin_end<T>, meta::has_key_value_pair<T>, meta::neg<meta::any<std::is_base_of<reference, T>, std::is_base_of<stack_reference, T>>>>::value>> {
			static int push(lua_State* L, const T& cont) {
				lua_createtable(L, static_cast<int>(cont.size()), 0);
				int tableindex = lua_gettop(L);
				for (auto&& pair : cont) {
					set_field(L, pair.first, pair.second, tableindex);
				}
				return 1;
			}
		};

		template<typename T>
		struct pusher<T, std::enable_if_t<std::is_base_of<reference, T>::value || std::is_base_of<stack_reference, T>::value>> {
			static int push(lua_State*, T& ref) {
				return ref.push();
			}

			static int push(lua_State*, T&& ref) {
				return ref.push();
			}
		};

		template<>
		struct pusher<bool> {
			static int push(lua_State* L, bool b) {
				lua_pushboolean(L, b);
				return 1;
			}
		};

		template<>
		struct pusher<nil_t> {
			static int push(lua_State* L, nil_t) {
				lua_pushnil(L);
				return 1;
			}
		};

		template<>
		struct pusher<metatable_key_t> {
			static int push(lua_State* L, metatable_key_t) {
				lua_pushlstring(L, "__mt", 4);
				return 1;
			}
		};

		template<>
		struct pusher<std::remove_pointer_t<lua_CFunction>> {
			static int push(lua_State* L, lua_CFunction func, int n = 0) {
				lua_pushcclosure(L, func, n);
				return 1;
			}
		};

		template<>
		struct pusher<lua_CFunction> {
			static int push(lua_State* L, lua_CFunction func, int n = 0) {
				lua_pushcclosure(L, func, n);
				return 1;
			}
		};

		template<>
		struct pusher<c_closure> {
			static int push(lua_State* L, c_closure cc) {
				lua_pushcclosure(L, cc.c_function, cc.upvalues);
				return 1;
			}
		};

		template<typename Arg, typename... Args>
		struct pusher<closure<Arg, Args...>> {
			template <std::size_t... I, typename T>
			static int push(std::index_sequence<I...>, lua_State* L, T&& c) {
				int pushcount = multi_push(L, detail::forward_get<I>(c.upvalues)...);
				return stack::push(L, c_closure(c.c_function, pushcount));
			}

			template <typename T>
			static int push(lua_State* L, T&& c) {
				return push(std::make_index_sequence<1 + sizeof...(Args)>(), L, std::forward<T>(c));
			}
		};

		template<>
		struct pusher<void*> {
			static int push(lua_State* L, void* userdata) {
				lua_pushlightuserdata(L, userdata);
				return 1;
			}
		};

		template<>
		struct pusher<lightuserdata_value> {
			static int push(lua_State* L, lightuserdata_value userdata) {
				lua_pushlightuserdata(L, userdata);
				return 1;
			}
		};

		template<typename T>
		struct pusher<light<T>> {
			static int push(lua_State* L, light<T> l) {
				lua_pushlightuserdata(L, static_cast<void*>(l.value));
				return 1;
			}
		};

		template<typename T>
		struct pusher<user<T>> {
			template <bool with_meta = true, typename... Args>
			static int push_with(lua_State* L, Args&&... args) {
				// A dumb pusher
				void* rawdata = lua_newuserdata(L, sizeof(T));
				T* data = static_cast<T*>(rawdata);
				std::allocator<T> alloc;
				alloc.construct(data, std::forward<Args>(args)...);
				if (with_meta) {
					const auto name = &usertype_traits<meta::unqualified_t<T>>::user_gc_metatable[0];
					lua_CFunction cdel = stack_detail::alloc_destroy<T>;
					// Make sure we have a plain GC set for this data
					if (luaL_newmetatable(L, name) != 0) {
						lua_pushlightuserdata(L, rawdata);
						lua_pushcclosure(L, cdel, 1);
						lua_setfield(L, -2, "__gc");
					}
					lua_setmetatable(L, -2);
				}
				return 1;
			}

			template <typename Arg, typename... Args, meta::disable<std::is_same<no_metatable_t, meta::unqualified_t<Arg>>> = meta::enabler>
			static int push(lua_State* L, Arg&& arg, Args&&... args) {
				return push_with(L, std::forward<Arg>(arg), std::forward<Args>(args)...);
			}

			template <typename... Args>
			static int push(lua_State* L, no_metatable_t, Args&&... args) {
				return push_with<false>(L, std::forward<Args>(args)...);
			}

			static int push(lua_State* L, const user<T>& u) {
				return push_with(L, u.value);
			}

			static int push(lua_State* L, user<T>&& u) {
				return push_with(L, std::move(u.value));
			}

			static int push(lua_State* L, no_metatable_t, const user<T>& u) {
				return push_with<false>(L, u.value);
			}

			static int push(lua_State* L, no_metatable_t, user<T>&& u) {
				return push_with<false>(L, std::move(u.value));
			}
		};

		template<>
		struct pusher<userdata_value> {
			static int push(lua_State* L, userdata_value data) {
				void** ud = static_cast<void**>(lua_newuserdata(L, sizeof(void*)));
				*ud = data.value;
				return 1;
			}
		};

		template<>
		struct pusher<const char*> {
			static int push_sized(lua_State* L, const char* str, std::size_t len) {
				lua_pushlstring(L, str, len);
				return 1;
			}
			
			static int push(lua_State* L, const char* str) {
				return push_sized(L, str, std::char_traits<char>::length(str));
			}

			static int push(lua_State* L, const char* str, std::size_t len) {
				return push_sized(L, str, len);
			}
		};

		template<size_t N>
		struct pusher<char[N]> {
			static int push(lua_State* L, const char(&str)[N]) {
				lua_pushlstring(L, str, N - 1);
				return 1;
			}
		};

		template <>
		struct pusher<char> {
			static int push(lua_State* L, char c) {
				const char str[2] = { c, '\0' };
				return stack::push(L, str);
			}
		};

		template<>
		struct pusher<std::string> {
			static int push(lua_State* L, const std::string& str) {
				lua_pushlstring(L, str.c_str(), str.size());
				return 1;
			}
		};

		template<>
		struct pusher<meta_function> {
			static int push(lua_State* L, meta_function m) {
				const std::string& str = name_of(m);
				lua_pushlstring(L, str.c_str(), str.size());
				return 1;
			}
		};

#if 0

		template<>
		struct pusher<const wchar_t*> {
			static int push(lua_State* L, const wchar_t* wstr) {
				return push(L, wstr, wstr + std::char_traits<wchar_t>::length(wstr));
			}
			static int push(lua_State* L, const wchar_t* wstrb, const wchar_t* wstre) {
				std::string str{};
				return stack::push(L, str);
			}
		};

		template<>
		struct pusher<const char16_t*> {
			static int push(lua_State* L, const char16_t* u16str) {
				return push(L, u16str, u16str + std::char_traits<char16_t>::length(u16str));
			}
			static int push(lua_State* L, const char16_t* u16strb, const char16_t* u16stre) {
				std::string str{};
				return stack::push(L, str);
			}
		};

		template<>
		struct pusher<const char32_t*> {
			static int push(lua_State* L, const char32_t* u32str) {
				return push(L, u32str, u32str + std::char_traits<char32_t>::length(u32str));
			}
			static int push(lua_State* L, const char32_t* u32strb, const char32_t* u32stre) {
				std::string str{};
				return stack::push(L, str);
			}
		};

		template<size_t N>
		struct pusher<wchar_t[N]> {
			static int push(lua_State* L, const wchar_t(&str)[N]) {
				return stack::push<const wchar_t*>(L, str, str + N - 1);
			}
		};

		template<size_t N>
		struct pusher<char16_t[N]> {
			static int push(lua_State* L, const char16_t(&str)[N]) {
				return stack::push<const char16_t*>(L, str, str + N - 1);
			}
		};

		template<size_t N>
		struct pusher<char32_t[N]> {
			static int push(lua_State* L, const char32_t(&str)[N]) {
				return stack::push<const char32_t*>(L, str, str + N - 1);
			}
		};

		template <>
		struct pusher<wchar_t> {
			static int push(lua_State* L, wchar_t c) {
				const wchar_t str[2] = { c, '\0' };
				return stack::push(L, str);
			}
		};

		template <>
		struct pusher<char16_t> {
			static int push(lua_State* L, char16_t c) {
				const char16_t str[2] = { c, '\0' };
				return stack::push(L, str);
			}
		};

		template <>
		struct pusher<char32_t> {
			static int push(lua_State* L, char32_t c) {
				const char32_t str[2] = { c, '\0' };
				return stack::push(L, str);
			}
		};

		template<>
		struct pusher<std::wstring> {
			static int push(lua_State* L, const std::wstring& wstr) {
				return stack::push(L, wstr.data(), wstr.data() + wstr.size());
			}
		};

		template<>
		struct pusher<std::u16string> {
			static int push(lua_State* L, const std::u16string& u16str) {
				return stack::push(L, u16str.data(), u16str.data() + u16str.size());
			}
		};

		template<>
		struct pusher<std::u32string> {
			static int push(lua_State* L, const std::u32string& u32str) {
				return stack::push(L, u32str.data(), u32str.data() + u32str.size());
			}
		};

#endif // Bad conversions

		template<typename... Args>
		struct pusher<std::tuple<Args...>> {
			template <std::size_t... I, typename T>
			static int push(std::index_sequence<I...>, lua_State* L, T&& t) {
				int pushcount = 0;
				(void)detail::swallow{ 0, (pushcount += stack::push(L,
					  detail::forward_get<I>(t)
				), 0)... };
				return pushcount;
			}

			template <typename T>
			static int push(lua_State* L, T&& t) {
				return push(std::index_sequence_for<Args...>(), L, std::forward<T>(t));
			}
		};

		template<typename A, typename B>
		struct pusher<std::pair<A, B>> {
			template <typename T>
			static int push(lua_State* L, T&& t) {
				int pushcount = stack::push(L, detail::forward_get<0>(t));
				pushcount += stack::push(L, detail::forward_get<1>(t));
				return pushcount;
			}
		};

		template<typename O>
		struct pusher<optional<O>> {
			template <typename T>
			static int push(lua_State* L, T&& t) {
				if (t == nullopt) {
					return stack::push(L, nullopt);
				}
				return stack::push(L, t.value());
			}
		};

		template<>
		struct pusher<nullopt_t> {
			static int push(lua_State* L, nullopt_t) {
				return stack::push(L, nil);
			}
		};

		template<>
		struct pusher<std::nullptr_t> {
			static int push(lua_State* L, std::nullptr_t) {
				return stack::push(L, nil);
			}
		};

		template<>
		struct pusher<this_state> {
			static int push(lua_State*, const this_state&) {
				return 0;
			}
		};
	} // stack
} // sol

#endif // SOL_STACK_PUSH_HPP
