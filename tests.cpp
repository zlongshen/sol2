#define CATCH_CONFIG_MAIN
#define SOL_CHECK_ARGUMENTS

#include <catch.hpp>
#include <sol.hpp>
#include <vector>
#include <map>
#include <unordered_map>
#include <iostream>
#include "test_stack_guard.hpp"

bool func_opt_ret_bool(sol::optional<int> i) {
	if (i)
		std::cout << i.value() << std::endl;
	else
		std::cout << "optional isn't set" << std::endl;
	return true;
}

TEST_CASE("table/traversal", "ensure that we can chain requests and tunnel down into a value if we desire") {

	sol::state lua;
	int begintop = 0, endtop = 0;

	sol::function scriptload = lua.load("t1 = {t2 = {t3 = 24}};");
	scriptload();
	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		int traversex24 = lua.traverse_get<int>("t1", "t2", "t3");
		REQUIRE(traversex24 == 24);
	} REQUIRE(begintop == endtop);

	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		int x24 = lua["t1"]["t2"]["t3"];
		REQUIRE(x24 == 24);
	} REQUIRE(begintop == endtop);

	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		lua["t1"]["t2"]["t3"] = 64;
		int traversex64 = lua.traverse_get<int>("t1", "t2", "t3");
		REQUIRE(traversex64 == 64);
	} REQUIRE(begintop == endtop);

	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		int x64 = lua["t1"]["t2"]["t3"];
		REQUIRE(x64 == 64);
	} REQUIRE(begintop == endtop);

	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		lua.traverse_set("t1", "t2", "t3", 13);
		int traversex13 = lua.traverse_get<int>("t1", "t2", "t3");
		REQUIRE(traversex13 == 13);
	} REQUIRE(begintop == endtop);

	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		int x13 = lua["t1"]["t2"]["t3"];
		REQUIRE(x13 == 13);
	} REQUIRE(begintop == endtop);
}

TEST_CASE("simple/set", "Check if the set works properly.") {
	sol::state lua;
	int begintop = 0, endtop = 0;
	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		lua.set("a", 9);
	} REQUIRE(begintop == endtop);
	REQUIRE_NOTHROW(lua.script("if a ~= 9 then error('wrong value') end"));
	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		lua.set("d", "hello");
	} REQUIRE(begintop == endtop);
	REQUIRE_NOTHROW(lua.script("if d ~= 'hello' then error('expected \\'hello\\', got '.. tostring(d)) end"));

	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		lua.set("e", std::string("hello"), "f", true);
	} REQUIRE(begintop == endtop);
	REQUIRE_NOTHROW(lua.script("if d ~= 'hello' then error('expected \\'hello\\', got '.. tostring(d)) end"));
	REQUIRE_NOTHROW(lua.script("if f ~= true then error('wrong value') end"));
}

TEST_CASE("simple/get", "Tests if the get function works properly.") {
	sol::state lua;
	int begintop = 0, endtop = 0;

	lua.script("a = 9");
	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		auto a = lua.get<int>("a");
		REQUIRE(a == 9.0);
	} REQUIRE(begintop == endtop);

	lua.script("b = nil");
	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		REQUIRE_NOTHROW(lua.get<sol::nil_t>("b"));
	} REQUIRE(begintop == endtop);

	lua.script("d = 'hello'");
	lua.script("e = true");
	{
		test_stack_guard g(lua.lua_state(), begintop, endtop);
		std::string d;
		bool e;
		std::tie(d, e) = lua.get<std::string, bool>("d", "e");
		REQUIRE(d == "hello");
		REQUIRE(e == true);
	} REQUIRE(begintop == endtop);
}

TEST_CASE("simple/set-get-global-integer", "Tests if the get function works properly with global integers") {
	sol::state lua;
	lua[1] = 25.4;
	lua.script("b = 1");
	double a = lua.get<double>(1);
	double b = lua.get<double>("b");
	REQUIRE(a == 25.4);
	REQUIRE(b == 1);
}

TEST_CASE("simple/get_or", "check if table.get_or works correctly") {
	sol::state lua;

	auto bob_table = lua.create_table("bob");
	bob_table.set("is_set", 42);

	int is_set = bob_table.get_or("is_set", 3);
	int is_not_set = bob_table.get_or("is_not_set", 22);

	REQUIRE(is_set == 42);
	REQUIRE(is_not_set == 22);

	lua["joe"] = 55.6;
	double bark = lua.get_or<double>("joe", 60);
	REQUIRE(bark == 55.6);
}

TEST_CASE("simple/proxy_get_or", "check if proxy.get_or works correctly") {
	sol::state lua;

	auto bob_table = lua.create_table("bob");
	bob_table.set("is_set", 42);

	int is_set = bob_table["is_set"].get_or(3);
	int is_not_set = bob_table["is_not_set"].get_or(22);

	REQUIRE(is_set == 42);
	REQUIRE(is_not_set == 22);

	lua["joe"] = 55.6;
	double bark = lua["joe"].get_or<double>(60);
	REQUIRE(bark == 55.6);
}

TEST_CASE("simple/addition", "check if addition works and can be gotten through lua.get and lua.set") {
	sol::state lua;

	lua.set("b", 0.2);
	lua.script("c = 9 + b");
	auto c = lua.get<double>("c");

	REQUIRE(c == 9.2);
}

TEST_CASE("simple/if", "check if if statements work through lua") {
	sol::state lua;

	std::string program = "if true then f = 0.1 else f = 'test' end";
	lua.script(program);
	auto f = lua.get<double>("f");

	REQUIRE(f == 0.1);
	REQUIRE((f == lua["f"]));
}

TEST_CASE("negative/basic_errors", "Check if error handling works correctly") {
	sol::state lua;

	REQUIRE_THROWS(lua.script("nil[5]"));
}

TEST_CASE("libraries", "Check if we can open libraries") {
	sol::state lua;
	REQUIRE_NOTHROW(lua.open_libraries(sol::lib::base, sol::lib::os));
}

TEST_CASE("libraries2", "Check if we can open ALL the libraries") {
	sol::state lua;
	REQUIRE_NOTHROW(lua.open_libraries(sol::lib::base,
		sol::lib::bit32,
		sol::lib::coroutine,
		sol::lib::debug,
		sol::lib::ffi,
		sol::lib::jit,
		sol::lib::math,
		sol::lib::os,
		sol::lib::package,
		sol::lib::string,
		sol::lib::table));
}

TEST_CASE("interop/null-to-nil-and-back", "nil should be the given type when a pointer from C++ is returned as nullptr, and nil should result in nullptr in connected C++ code") {
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	lua.set_function("lol", []() -> int* {
		return nullptr;
	});
	lua.set_function("rofl", [](int* x) {
		std::cout << x << std::endl;
	});
	REQUIRE_NOTHROW(lua.script("x = lol()\n"
		"rofl(x)\n"
		"assert(x == nil)"));
}

TEST_CASE("utilities/this_state", "Ensure this_state argument can be gotten anywhere in the function.") {
	struct bark {
		int with_state(sol::this_state l, int a, int b) {
			lua_State* L = l;
			int c = lua_gettop(L);
			return a + b + (c - c);
		}

		static int with_state_2(int a, sol::this_state l, int b) {
			std::cout << "inside with_state_2" << std::endl;
			lua_State* L = l;
			std::cout << "L is" << (void*)L << std::endl;
			int c = lua_gettop(L);
			return a * b + (c - c);
		}
	};

	sol::state lua;
	std::cout << "created lua state" << std::endl;
	lua.open_libraries(sol::lib::base);
	lua.new_usertype<bark>("bark",
		"with_state", &bark::with_state
		);

	std::cout << "setting b and with_state_2" << std::endl;
	bark b;
	lua.set("b", &b);
	lua.set("with_state_2", bark::with_state_2);
	std::cout << "finished setting" << std::endl;
	std::cout << "getting fx" << std::endl;
	sol::function fx = lua["with_state_2"];
	std::cout << "calling fx" << std::endl;
	int a = fx(25, 25);
	std::cout << "finished setting fx" << std::endl;
	std::cout << "calling a script" << std::endl;
	lua.script("a = with_state_2(25, 25)");
	std::cout << "calling c script" << std::endl;
	lua.script("c = b:with_state(25, 25)");
	std::cout << "getting a" << std::endl;
	int la = lua["a"];
	std::cout << "getting b" << std::endl;
	int lc = lua["c"];

	REQUIRE(lc == 50);
	REQUIRE(a == 625);
	REQUIRE(la == 625);
}

TEST_CASE("object/conversions", "make sure all basic reference types can be made into objects") {
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	struct d {};

	lua.script("function f () print('bark') end");
	lua["d"] = d{};
	lua["l"] = static_cast<void*>(nullptr);

	sol::table t = lua.create_table();
	sol::thread th = sol::thread::create(lua);
	sol::function f = lua["f"];
	sol::protected_function pf = lua["f"];
	sol::userdata ud = lua["d"];
	sol::lightuserdata lud = lua["l"];

	sol::object ot(t);
	sol::object ot2 = ot;
	sol::object oth(th);
	sol::object of(f);
	sol::object opf(pf);
	sol::object od(ud);
	sol::object ol(lud);

	auto oni = sol::make_object(lua, 50);
	auto ond = sol::make_object(lua, 50.0);

	std::string somestring = "look at this text isn't it nice";
	auto osl = sol::make_object(lua, "Bark bark bark");
	auto os = sol::make_object(lua, somestring);

	auto omn = sol::make_object(lua, sol::nil);

	REQUIRE(ot.get_type() == sol::type::table);
	REQUIRE(ot2.get_type() == sol::type::table);
	REQUIRE(oth.get_type() == sol::type::thread);
	REQUIRE(of.get_type() == sol::type::function);
	REQUIRE(opf.get_type() == sol::type::function);
	REQUIRE(od.get_type() == sol::type::userdata);
	REQUIRE(ol.get_type() == sol::type::lightuserdata);
	REQUIRE(oni.get_type() == sol::type::number);
	REQUIRE(ond.get_type() == sol::type::number);
	REQUIRE(osl.get_type() == sol::type::string);
	REQUIRE(os.get_type() == sol::type::string);
	REQUIRE(omn.get_type() == sol::type::nil);
}

TEST_CASE("state/require_script", "opening strings as 'requires' clauses") {
	std::string code = "return { modfunc = function () return 221 end }";

	sol::state lua;
	sol::table thingy1 = lua.require_script("thingy", code);
	sol::table thingy2 = lua.require_script("thingy", code);

	int val1 = thingy1["modfunc"]();
	int val2 = thingy2["modfunc"]();
	REQUIRE(val1 == 221);
	REQUIRE(val2 == 221);
	// must have loaded the same table
	REQUIRE(thingy1 == thingy2);
}

TEST_CASE("state/require", "opening using a file") {
	struct open {
		static int open_func(lua_State* L) {
			sol::state_view lua = L;
			return sol::stack::push(L, lua.create_table_with("modfunc", sol::as_function([]() { return 221; })));
		}
	};

	sol::state lua;
	sol::table thingy1 = lua.require("thingy", open::open_func);
	sol::table thingy2 = lua.require("thingy", open::open_func);

	int val1 = thingy1["modfunc"]();
	int val2 = thingy2["modfunc"]();
	REQUIRE(val1 == 221);
	REQUIRE(val2 == 221);
	// THIS IS ONLY REQUIRED IN LUA 5.3, FOR SOME REASON
	// must have loaded the same table
	// REQUIRE(thingy1 == thingy2);   
}

TEST_CASE("state/multi-require", "make sure that requires transfers across hand-rolled script implementation and standard requiref") {
	struct open {
		static int open_func(lua_State* L) {
			sol::state_view lua = L;
			return sol::stack::push(L, lua.create_table_with("modfunc", sol::as_function([]() { return 221; })));
		}
	};

	std::string code = "return { modfunc = function () return 221 end }";
	sol::state lua;
	sol::table thingy1 = lua.require("thingy", open::open_func);
	sol::table thingy2 = lua.require("thingy", open::open_func);
	sol::table thingy3 = lua.require_script("thingy", code);

	int val1 = thingy1["modfunc"]();
	int val2 = thingy2["modfunc"]();
	int val3 = thingy3["modfunc"]();
	REQUIRE(val1 == 221);
	REQUIRE(val2 == 221);
	REQUIRE(val3 == 221);
	// must have loaded the same table
	// Lua is not obliged to give a shit. Thanks, Lua
	//REQUIRE(thingy1 == thingy2);
	// But we care, thankfully
	//REQUIRE(thingy1 == thingy3);
	REQUIRE(thingy2 == thingy3);
}

TEST_CASE("feature/indexing-overrides", "make sure index functions can be overridden on types") {
	struct PropertySet {
		sol::object get_property_lua(const char* name, sol::this_state s)
		{
			auto& var = props[name];
			return sol::make_object(s, var);
		}

		void set_property_lua(const char* name, sol::stack_object object)
		{
			props[name] = object.as<std::string>();
		}

		std::unordered_map<std::string, std::string> props;
	};

	struct DynamicObject {
		PropertySet& get_dynamic_props() {
			return dynamic_props;
		}

		PropertySet dynamic_props;
	};

	sol::state lua;
	lua.open_libraries(sol::lib::base);

	lua.new_usertype<PropertySet>("PropertySet"
		, sol::meta_function::new_index, &PropertySet::set_property_lua
		, sol::meta_function::index, &PropertySet::get_property_lua
		);
	lua.new_usertype<DynamicObject>("DynamicObject"
		, "props", sol::property(&DynamicObject::get_dynamic_props)
		);

	lua.script(R"__(
obj = DynamicObject:new()
obj.props.name = 'test name'
print('name = ' .. obj.props.name)
)__");

	std::string name = lua["obj"]["props"]["name"];
	REQUIRE(name == "test name");
}

TEST_CASE("features/indexing-numbers", "make sure indexing functions can be override on usertypes") {
	class vector {
	public:
		double data[3];

		vector() : data{ 0,0,0 } {}

		double& operator[](int i)
		{
			return data[i];
		}


		static double my_index(vector& v, int i)
		{
			return v[i];
		}

		static void my_new_index(vector& v, int i, double x)
		{
			v[i] = x;
		}
	};

	sol::state lua;
	lua.open_libraries(sol::lib::base);
	lua.new_usertype<vector>("vector", sol::constructors<sol::types<>>(),
		sol::meta_function::index, &vector::my_index,
		sol::meta_function::new_index, &vector::my_new_index);
	lua.script("v = vector.new()\n"
		"print(v[1])\n"
		"v[2] = 3\n"
		"print(v[2])\n"
	);

	vector& v = lua["v"];
	REQUIRE(v[0] == 0.0);
	REQUIRE(v[1] == 0.0);
	REQUIRE(v[2] == 3.0);
}

TEST_CASE("features/multiple-inheritance", "Ensure that multiple inheritance works as advertised") {
	struct base1 {
		int a1 = 250;
	};

	struct base2 {
		int a2 = 500;
	};

	struct simple : base1 {

	};

	struct complex : base1, base2 {

	};


	sol::state lua;
	lua.open_libraries(sol::lib::base);
	lua.new_usertype<base1>("base1",
		"a1", &base1::a1
		);
	lua.new_usertype<base2>("base2",
		"a2", &base2::a2
		);
	lua.new_usertype<simple>("simple",
		"a1", &simple::a1,
		sol::base_classes, sol::bases<base1>()
		);
	lua.new_usertype<complex>("complex",
		"a1", &complex::a1,
		"a2", &complex::a2,
		sol::base_classes, sol::bases<base1, base2>()
		);
	lua.script("c = complex.new()\n"
		"s = simple.new()\n"
		"b1 = base1.new()\n"
		"b2 = base1.new()\n"
	);

	base1* sb1 = lua["s"];
	REQUIRE(sb1 != nullptr);
	REQUIRE(sb1->a1 == 250);

	base1* cb1 = lua["c"];
	base2* cb2 = lua["c"];

	REQUIRE(cb1 != nullptr);
	REQUIRE(cb2 != nullptr);
	REQUIRE(cb1->a1 == 250);
	REQUIRE(cb2->a2 == 500);
}


TEST_CASE("regressions/std::ref", "Ensure that std::reference_wrapper<> isn't considered as a function by using unwrap_unqualified_t trait") {
	struct base1 {
		int a1 = 250;
	};

	sol::state lua;
	base1 v;
	lua["vp"] = &v;
	lua["vr"] = std::ref(v);

	base1* vp = lua["vp"];
	base1& vr = lua["vr"];
	REQUIRE(vp != nullptr);
	REQUIRE(vp == &v);

	REQUIRE(vp->a1 == 250);
	REQUIRE(vr.a1 == 250);
	
	v.a1 = 568;

	REQUIRE(vp->a1 == 568);
	REQUIRE(vr.a1 == 568);
}

TEST_CASE("optional/left-out-args", "Make sure arguments can be left out of optional without tanking miserably") {

	sol::state lua;
	lua.open_libraries(sol::lib::base);

	// sol::optional needs an argument no matter what
	lua.set_function("func_opt_ret_bool", func_opt_ret_bool);
	REQUIRE_NOTHROW(
	lua.script(R"(
        func_opt_ret_bool(42)
        func_opt_ret_bool()
        print('ok')
        )");
	);
}
