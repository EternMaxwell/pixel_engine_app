﻿#pragma once

#include <concurrent_unordered_set.h>
#include <spdlog/spdlog.h>

#include <BS_thread_pool.hpp>
#include <any>
#include <deque>
#include <entt/entity/registry.hpp>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <pfr.hpp>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "pixel_engine/app/command.h"
#include "pixel_engine/app/event.h"
#include "pixel_engine/app/query.h"
#include "pixel_engine/app/resource.h"
#include "pixel_engine/app/scheduler.h"
#include "pixel_engine/app/state.h"
#include "pixel_engine/app/system.h"

namespace pixel_engine {
namespace entity {
class Plugin {
   public:
    virtual void build(App& app) = 0;
};

class LoopPlugin;

struct AppExit {};

bool check_exit(EventReader<AppExit> exit_events);

void exit_app(EventWriter<AppExit> exit_events);

struct SystemNode {
    const bool in_main_thread;
    std::shared_ptr<Scheduler> scheduler;
    const type_info* scheduler_type;
    std::shared_ptr<BasicSystem<void>> system;
    std::unordered_set<std::shared_ptr<condition>> conditions;
    std::unordered_set<std::shared_ptr<SystemNode>> user_defined_before;
    std::unordered_set<std::shared_ptr<SystemNode>> app_generated_before;
    std::unordered_map<size_t, std::any> sets;
    double avg_reach_time = 0;

    SystemNode(
        std::shared_ptr<Scheduler> scheduler,
        std::shared_ptr<BasicSystem<void>> system,
        const std::type_info* scheduler_type, const bool& in_main = false)
        : scheduler(scheduler),
          system(system),
          in_main_thread(in_main),
          scheduler_type(scheduler_type) {}

    std::tuple<std::shared_ptr<Scheduler>, std::shared_ptr<BasicSystem<void>>>
    to_tuple() {
        return std::make_tuple(scheduler, system);
    }
    /*! @brief Get the depth of a node, 0 if it is a leaf node.
     * @return The depth of the node.
     */
    size_t user_before_depth();
    /*! @brief Get the time in milliseconds for the runner to reach this system.
     * @return The time in milliseconds.
     */
    double time_to_reach();
};

struct SystemRunner {
   public:
    SystemRunner(App* app, std::shared_ptr<BS::thread_pool> pool)
        : app(app), ignore_scheduler(false), pool(pool) {}
    SystemRunner(
        App* app, std::shared_ptr<BS::thread_pool> pool, bool ignore_scheduler)
        : app(app), ignore_scheduler(ignore_scheduler), pool(pool) {}
    SystemRunner(const SystemRunner& other) {
        app = other.app;
        ignore_scheduler = other.ignore_scheduler;
        tails = other.tails;
        pool = other.pool;
    }
    void run();
    void add_system(std::shared_ptr<SystemNode> node) {
        systems_all.push_back(node);
    }
    void prepare();
    void reset() { futures.clear(); }
    void wait() { pool->wait(); }
    void sort_time() {
        std::sort(
            systems_all.begin(), systems_all.end(),
            [](const auto& a, const auto& b) {
                return a->time_to_reach() < b->time_to_reach();
            });
    }
    void sort_depth() {
        std::sort(
            systems_all.begin(), systems_all.end(),
            [](const auto& a, const auto& b) {
                return a->user_before_depth() < b->user_before_depth();
            });
    }
    size_t system_count() { return systems_all.size(); }

   private:
    App* app;
    bool ignore_scheduler;
    bool runned = false;
    std::condition_variable cv;
    std::mutex m;
    std::atomic<bool> any_done = true;
    std::unordered_set<std::shared_ptr<SystemNode>> tails;
    std::vector<std::shared_ptr<SystemNode>> systems_all;
    std::unordered_map<std::shared_ptr<SystemNode>, std::future<void>> futures;
    std::shared_ptr<BS::thread_pool> pool;
    std::shared_ptr<SystemNode> should_run(
        const std::shared_ptr<SystemNode>& sys);
    std::shared_ptr<SystemNode> get_next();
    bool done();
    bool all_load();
};

template <typename T, typename Tuple>
struct tuple_contain;

template <typename T>
struct tuple_contain<T, std::tuple<>> : std::false_type {};

template <typename T, typename U, typename... Ts>
struct tuple_contain<T, std::tuple<U, Ts...>>
    : tuple_contain<T, std::tuple<Ts...>> {};

template <typename T, typename... Ts>
struct tuple_contain<T, std::tuple<T, Ts...>> : std::true_type {};

template <template <typename...> typename T, typename Tuple>
struct tuple_contain_template;

template <template <typename...> typename T>
struct tuple_contain_template<T, std::tuple<>> : std::false_type {};

template <template <typename...> typename T, typename U, typename... Ts>
struct tuple_contain_template<T, std::tuple<U, Ts...>>
    : tuple_contain_template<T, std::tuple<Ts...>> {};

template <template <typename...> typename T, typename... Ts, typename... Temps>
struct tuple_contain_template<T, std::tuple<T<Temps...>, Ts...>>
    : std::true_type {};

template <template <typename...> typename T, typename Tuple>
struct tuple_template_index {};

template <template <typename...> typename T, typename U, typename... Args>
struct tuple_template_index<T, std::tuple<U, Args...>> {
    static constexpr int index() {
        if constexpr (is_template_of<T, U>::value) {
            return 0;
        } else {
            return 1 + tuple_template_index<T, std::tuple<Args...>>::index();
        }
    }
};

struct before {
    friend class App;

   private:
    std::vector<std::shared_ptr<SystemNode>> nodes;

   public:
    before() {};
    template <typename... Nodes>
    before(Nodes... nodes) : nodes({nodes...}){};
};

struct after {
    friend class App;

   private:
    std::vector<std::shared_ptr<SystemNode>> nodes;

   public:
    after() {};
    template <typename... Nodes>
    after(Nodes... nodes) : nodes({nodes...}){};
};

template <typename... Args>
struct in_set {
    std::unordered_map<size_t, std::any> sets;
    in_set(Args... args)
        : sets({{typeid(Args).hash_code(), std::any(args)}...}) {}
    template <typename T, typename... Args>
    in_set(in_set<T, Args...> in_sets) : sets(in_sets.sets) {}
    in_set(std::unordered_map<size_t, std::any> sets) : sets(sets) {}
};

class App {
    friend class LoopPlugin;

   protected:
    bool m_loop_enabled = false;
    entt::registry m_registry;
    std::unordered_map<entt::entity, std::set<entt::entity>>
        m_entity_relation_tree;
    std::unordered_map<size_t, std::any> m_resources;
    std::unordered_map<size_t, std::shared_ptr<std::deque<Event>>> m_events;
    std::unordered_map<size_t, std::vector<std::any>> m_system_sets;
    std::unordered_map<size_t, std::vector<std::shared_ptr<SystemNode>>>
        m_in_set_systems;
    std::vector<Command> m_existing_commands;
    std::vector<std::unique_ptr<BasicSystem<void>>> m_state_update;
    std::vector<std::shared_ptr<SystemNode>> m_systems;
    std::unordered_map<size_t, std::shared_ptr<Plugin>> m_plugins;
    std::unordered_map<size_t, std::shared_ptr<SystemRunner>> m_runners;
    std::shared_ptr<BS::thread_pool> m_pool =
        std::make_shared<BS::thread_pool>(4);

    void enable_loop() { m_loop_enabled = true; }
    void disable_loop() { m_loop_enabled = false; }

    template <typename T, typename... Args>
    T tuple_get(std::tuple<Args...> tuple) {
        if constexpr (tuple_contain<T, std::tuple<Args...>>::value) {
            return std::get<T>(tuple);
        } else {
            if constexpr (std::is_same_v<T, std::shared_ptr<SystemNode>*>) {
                return nullptr;
            }
            return T();
        }
    }

    template <template <typename...> typename T, typename... Args>
    constexpr auto tuple_get_template(std::tuple<Args...> tuple) {
        if constexpr (tuple_contain_template<T, std::tuple<Args...>>::value) {
            return std::get<
                tuple_template_index<T, std::tuple<Args...>>::index()>(tuple);
        } else {
            return T();
        }
    }

    template <typename T>
    struct value_type {
        static T get(App* app) { static_assert(1, "value type not valid."); }
    };

    template <>
    struct value_type<Command> {
        static Command get(App* app) {
            return Command(
                &app->m_registry, &app->m_entity_relation_tree,
                &app->m_resources, &app->m_events);
        }
    };

    template <typename... Gets, typename... Withs, typename... Exs>
    struct value_type<Query<Get<Gets...>, With<Withs...>, Without<Exs...>>> {
        static Query<Get<Gets...>, With<Withs...>, Without<Exs...>> get(
            App* app) {
            return Query<Get<Gets...>, With<Withs...>, Without<Exs...>>(
                app->m_registry);
        }
    };

    template <typename Res>
    struct value_type<Resource<Res>> {
        static Resource<Res> get(App* app) {
            if (app->m_resources.find(typeid(Res).hash_code()) !=
                app->m_resources.end()) {
                return Resource<Res>(
                    &app->m_resources[typeid(Res).hash_code()]);
            } else {
                return Resource<Res>();
            }
        }
    };

    template <typename Evt>
    struct value_type<EventWriter<Evt>> {
        static EventWriter<Evt> get(App* app) {
            if (app->m_events.find(typeid(Evt).hash_code()) ==
                app->m_events.end()) {
                app->m_events[typeid(Evt).hash_code()] =
                    std::make_shared<std::deque<Event>>();
            }
            return EventWriter<Evt>(app->m_events[typeid(Evt).hash_code()]);
        }
    };

    template <typename Evt>
    struct value_type<EventReader<Evt>> {
        static EventReader<Evt> get(App* app) {
            if (app->m_events.find(typeid(Evt).hash_code()) ==
                app->m_events.end()) {
                app->m_events[typeid(Evt).hash_code()] =
                    std::make_shared<std::deque<Event>>();
            }
            return EventReader<Evt>(app->m_events[typeid(Evt).hash_code()]);
        }
    };

    /*!
     * @brief This is where the systems get their parameters by
     * type. All possible type should be handled here.
     */
    template <typename... Args>
    std::tuple<Args...> get_values() {
        return std::make_tuple(value_type<Args>::get(this)...);
    }

    template <typename T>
    auto state_update() {
        return
            [&](Resource<State<T>> state, Resource<NextState<T>> state_next) {
                if (state.has_value() && state_next.has_value()) {
                    state->just_created = false;
                    state->m_state = state_next->m_state;
                }
            };
    }

    void end_commands();

    void tick_events();

    void update_states();

    template <typename SchT, typename T, typename... Args>
    void configure_system_sets(
        std::shared_ptr<SystemNode> node, in_set<T, Args...> in_sets) {
        T& set_of_T = std::any_cast<T&>(in_sets.sets[typeid(T).hash_code()]);
        if (m_system_sets.find(typeid(T).hash_code()) != m_system_sets.end()) {
            bool before = true;
            for (auto& set_any : m_system_sets[typeid(T).hash_code()]) {
                T& set = std::any_cast<T&>(set_any);
                if (set == set_of_T) {
                    before = false;
                    break;
                }
                for (auto& systems : m_in_set_systems[typeid(T).hash_code()]) {
                    if (systems->sets.find(typeid(T).hash_code()) !=
                        systems->sets.end()) {
                        T& sys_set = std::any_cast<T&>(
                            systems->sets[typeid(T).hash_code()]);
                        if (sys_set == set &&
                            dynamic_cast<SchT*>(systems->scheduler.get()) !=
                                NULL) {
                            if (before) {
                                node->user_defined_before.insert(systems);
                            } else {
                                systems->user_defined_before.insert(node);
                            }
                        }
                    }
                }
            }
        }
        m_in_set_systems[typeid(T).hash_code()].push_back(node);
        node->sets[typeid(T).hash_code()] = set_of_T;
        if constexpr (sizeof...(Args) > 0) {
            configure_system_sets<SchT>(node, in_set<Args...>(in_sets.sets));
        }
    }

    template <typename T>
    void configure_system_sets(
        std::shared_ptr<SystemNode> node, in_set<> in_sets) {}

    template <typename T>
    void load_runner() {
        auto runner = std::make_shared<SystemRunner>(this, m_pool);
        for (auto& node : m_systems) {
            auto& scheduler = node->scheduler;
            if (scheduler != nullptr &&
                dynamic_cast<T*>(scheduler.get()) != NULL) {
                runner->add_system(node);
            }
        }
        m_runners.insert(std::make_pair(typeid(T).hash_code(), runner));
    }

    template <typename T>
    void prepare_runner() {
        m_runners[typeid(T).hash_code()]->prepare();
    }

    template <typename T>
    void run_runner() {
        m_runners[typeid(T).hash_code()]->run();
        m_runners[typeid(T).hash_code()]->wait();
        m_runners[typeid(T).hash_code()]->reset();
        end_commands();
    }

    template <typename... Schs>
    void load_runners() {
        (load_runner<Schs>(), ...);
    }

    template <typename... Schs>
    void prepare_runners() {
        (prepare_runner<Schs>(), ...);
    }

    template <typename... Schs>
    void run_runners() {
        (run_runner<Schs>(), ...);
    }

   public:
    App() {}

    /*! @brief Get the registry.
     * @return The registry.
     */
    const auto& registry() { return m_registry; }

    /*! @brief Get a command object on this app.
     * @return The command object.
     */
    Command command() {
        return Command(
            &m_registry, &m_entity_relation_tree, &m_resources, &m_events);
    }

    /*! @brief Run a system.
     * @tparam Args The types of the arguments for the system.
     * @param func The system to be run.
     * @return The App object itself.
     */
    template <typename... Args>
    App& run_system(std::function<void(Args...)> func) {
        std::apply(func, get_values<Args...>());
        return *this;
    }

    /*! @brief Run a system.
     * @tparam Args1 The types of the arguments for the system.
     * @tparam Args2 The types of the arguments for the condition.
     * @param func The system to be run.
     * @param condition The condition for the system to be run.
     * @return The App object itself.
     */
    template <typename T, typename... Args>
    T run_system_v(std::function<T(Args...)> func) {
        return std::apply(func, get_values<Args...>());
    }

    void check_locked(
        std::shared_ptr<SystemNode> node, std::shared_ptr<SystemNode>& node2);

    /*! @brief Configure system sets. This means the sequence of the args is the
     * sequence of the sets. And this affects how systems are run.
     *  @tparam Sch The scheduler type.
     *  @tparam T The type of the system set.
     *  @param arg The system set.
     *  @param args The rest of the system sets.
     *  @return The App object itself.
     */
    template <typename T, typename... Args>
    App& configure_sets(T arg, Args... args) {
        m_system_sets[typeid(T).hash_code()] = {arg, args...};
        std::vector<std::vector<std::shared_ptr<SystemNode>>> in_set_systems;
        for (auto& set_any : m_system_sets[typeid(T).hash_code()]) {
            T& set = std::any_cast<T&>(set_any);
            in_set_systems.push_back({});
            auto& systems_in_this_set = in_set_systems.back();
            for (auto& systems : m_in_set_systems[typeid(T).hash_code()]) {
                if (systems->sets.find(typeid(T).hash_code()) !=
                    systems->sets.end()) {
                    T& sys_set =
                        std::any_cast<T&>(systems->sets[typeid(T).hash_code()]);
                    if (sys_set == set) {
                        systems_in_this_set.push_back(systems);
                    }
                }
            }
        }
        for (int i = 0; i < in_set_systems.size(); i++) {
            for (int j = i + 1; j < in_set_systems.size(); j++) {
                for (auto& system1 : in_set_systems[i]) {
                    for (auto& system2 : in_set_systems[j]) {
                        if (system1->scheduler_type != system2->scheduler_type)
                            continue;
                        system2->user_defined_before.insert(system1);
                    }
                }
            }
        }
        return *this;
    }

    /*! @brief Add a system.
     * @tparam Args The types of the arguments for the system.
     * @param scheduler The scheduler for the system.
     * @param func The system to be run.
     * @param befores The systems that should run after this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param afters The systems that should run before this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @return The App object itself.
     */
    template <typename Sch, typename... Args, typename... Sets>
    App& add_system_inner(
        Sch scheduler, std::function<void(Args...)> func,
        std::shared_ptr<SystemNode>* node = nullptr, before befores = before(),
        after afters = after(),
        std::unordered_set<std::shared_ptr<condition>> conditions = {},
        in_set<Sets...> in_sets = in_set()) {
        std::shared_ptr<SystemNode> new_node = std::make_shared<SystemNode>(
            std::make_shared<Sch>(scheduler),
            std::make_shared<System<Args...>>(System<Args...>(this, func)),
            &typeid(Sch));
        new_node->conditions = conditions;
        for (auto& before_node : afters.nodes) {
            if ((before_node != nullptr) &&
                (dynamic_cast<Sch*>(before_node->scheduler.get()) != NULL)) {
                new_node->user_defined_before.insert(before_node);
            }
        }
        for (auto& after_node : befores.nodes) {
            if ((after_node != nullptr) &&
                (dynamic_cast<Sch*>(after_node->scheduler.get()) != NULL)) {
                after_node->user_defined_before.insert(new_node);
            }
        }
        if (node != nullptr) {
            *node = new_node;
        }
        configure_system_sets<Sch>(new_node, in_sets);
        check_locked(new_node, new_node);
        m_systems.push_back(new_node);
        return *this;
    }

    /*! @brief Add a system.
     * @tparam Args The types of the arguments for the system.
     * @param scheduler The scheduler for the system.
     * @param func The system to be run.
     * @param befores The systems that should run after this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param afters The systems that should run before this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param in_sets The sets that this system belongs to. This will affect the
     * sequence of the systems. Systems in same set type but different scheduler
     * will not be affected by this.
     * @return The App object itself.
     */
    template <typename Sch, typename... Args, typename... Ts>
    App& add_system(Sch scheduler, void (*func)(Args...), Ts... args) {
        auto args_tuple = std::tuple<Ts...>(args...);
        auto tuple = std::make_tuple(
            scheduler, std::function<void(Args...)>(func),
            tuple_get<std::shared_ptr<SystemNode>*>(args_tuple),
            tuple_get<before>(args_tuple), tuple_get<after>(args_tuple),
            tuple_get<std::unordered_set<std::shared_ptr<condition>>>(
                args_tuple),
            tuple_get_template<in_set>(args_tuple));
        std::apply([this](auto... args) { add_system_inner(args...); }, tuple);
        return *this;
    }

    /*! @brief Add a system.
     * @tparam Args The types of the arguments for the system.
     * @param scheduler The scheduler for the system.
     * @param func The system to be run.
     * @param befores The systems that should run after this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param afters The systems that should run before this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param in_sets The sets that this system belongs to. This will affect the
     * sequence of the systems. Systems in same set type but different scheduler
     * will not be affected by this.
     * @return The App object itself.
     */
    template <typename Sch, typename... Args, typename... Ts>
    App& add_system(
        Sch scheduler, std::function<void(Args...)> func, Ts... args) {
        auto args_tuple = std::tuple<Ts...>(args...);
        auto tuple = std::make_tuple(
            scheduler, func,
            tuple_get<std::shared_ptr<SystemNode>*>(args_tuple),
            tuple_get<before>(args_tuple), tuple_get<after>(args_tuple),
            tuple_get<std::unordered_set<std::shared_ptr<condition>>>(
                args_tuple),
            tuple_get_template<in_set>(args_tuple));
        std::apply([this](auto... args) { add_system_inner(args...); }, tuple);
        return *this;
    }

    /*! @brief Add a system.
     * @tparam Args The types of the arguments for the system.
     * @param scheduler The scheduler for the system.
     * @param func The system to be run.
     * @param befores The systems that should run after this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param afters The systems that should run before this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param in_sets The sets that this system belongs to. This will affect the
     * sequence of the systems. Systems in same set type but different scheduler
     * will not be affected by this.
     * @return The App object itself.
     */
    template <typename Sch, typename... Args, typename... Sets>
    App& add_system_main_inner(
        Sch scheduler, std::function<void(Args...)> func,
        std::shared_ptr<SystemNode>* node = nullptr, before befores = before(),
        after afters = after(),
        std::unordered_set<std::shared_ptr<condition>> conditions = {},
        in_set<Sets...> in_sets = in_set()) {
        std::shared_ptr<SystemNode> new_node = std::make_shared<SystemNode>(
            std::make_shared<Sch>(scheduler),
            std::make_shared<System<Args...>>(System<Args...>(this, func)),
            &typeid(Sch), true);
        new_node->conditions = conditions;
        for (auto& before_node : afters.nodes) {
            if ((before_node != nullptr) &&
                (dynamic_cast<Sch*>(before_node->scheduler.get()) != NULL)) {
                new_node->user_defined_before.insert(before_node);
            }
        }
        for (auto& after_node : befores.nodes) {
            if ((after_node != nullptr) &&
                (dynamic_cast<Sch*>(after_node->scheduler.get()) != NULL)) {
                after_node->user_defined_before.insert(new_node);
            }
        }
        if (node != nullptr) {
            *node = new_node;
        }
        configure_system_sets<Sch>(new_node, in_sets);
        check_locked(new_node, new_node);
        m_systems.push_back(new_node);
        return *this;
    }

    /*! @brief Add a system.
     * @tparam Args The types of the arguments for the system.
     * @param scheduler The scheduler for the system.
     * @param func The system to be run.
     * @param befores The systems that should run after this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param afters The systems that should run before this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param in_sets The sets that this system belongs to. This will affect the
     * sequence of the systems. Systems in same set type but different scheduler
     * will not be affected by this.
     * @return The App object itself.
     */
    template <typename Sch, typename... Args, typename... Ts>
    App& add_system_main(Sch scheduler, void (*func)(Args...), Ts... args) {
        auto args_tuple = std::tuple<Ts...>(args...);
        auto tuple = std::make_tuple(
            scheduler, std::function<void(Args...)>(func),
            tuple_get<std::shared_ptr<SystemNode>*>(args_tuple),
            tuple_get<before>(args_tuple), tuple_get<after>(args_tuple),
            tuple_get<std::unordered_set<std::shared_ptr<condition>>>(
                args_tuple),
            tuple_get_template<in_set>(args_tuple));
        std::apply(
            [this](auto... args) { add_system_main_inner(args...); }, tuple);
        return *this;
    }

    /*! @brief Add a system.
     * @tparam Args The types of the arguments for the system.
     * @param scheduler The scheduler for the system.
     * @param func The system to be run.
     * @param befores The systems that should run after this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param afters The systems that should run before this system. If they are
     * not in same scheduler, this will be ignored when preparing runners.
     * @param in_sets The sets that this system belongs to. This will affect the
     * sequence of the systems. Systems in same set type but different scheduler
     * will not be affected by this.
     * @return The App object itself.
     */
    template <typename Sch, typename... Args, typename... Ts>
    App& add_system_main(
        Sch scheduler, std::function<void(Args...)> func, Ts... args) {
        auto args_tuple = std::tuple<Ts...>(args...);
        auto tuple = std::make_tuple(
            scheduler, func,
            tuple_get<std::shared_ptr<SystemNode>*>(args_tuple),
            tuple_get<before>(args_tuple), tuple_get<after>(args_tuple),
            tuple_get<std::unordered_set<std::shared_ptr<condition>>>(
                args_tuple),
            tuple_get_template<in_set>(args_tuple));
        std::apply(
            [this](auto... args) { add_system_main_inner(args...); }, tuple);
        return *this;
    }

    /*! @brief Add a plugin.
     * @param plugin The plugin to be added.
     * @return The App object itself.
     */
    template <
        typename T, std::enable_if_t<std::is_base_of_v<Plugin, T>>* = nullptr>
    App& add_plugin(T plugin) {
        if (m_plugins.find(typeid(T).hash_code()) != m_plugins.end()) {
            return *this;
        }
        plugin.build(*this);
        m_plugins[typeid(T).hash_code()] = std::make_shared<T>(plugin);
        Command cmd = command();
        cmd.insert_resource(plugin);
        return *this;
    }

    /*! @brief Get a plugin.
     * @tparam T The type of the plugin.
     * @return The plugin.
     */
    template <typename T>
    Resource<T> get_plugin() {
        Resource<T> plugin_res = value_type<Resource<T>>::get(this);
        return plugin_res;
    }

    /*! @brief Insert a state.
     * If the state already exists, nothing will happen.
     * @tparam T The type of the state.
     * @param state The state value to be set when inserted.
     * @return The App object itself.
     */
    template <typename T>
    App& insert_state(T state) {
        Command cmd = command();
        cmd.insert_resource(State(state));
        cmd.insert_resource(NextState(state));
        m_state_update.push_back(
            std::make_unique<
                System<Resource<State<T>>, Resource<NextState<T>>>>(
                System<Resource<State<T>>, Resource<NextState<T>>>(
                    this, state_update<T>())));
        return *this;
    }

    /*! @brief Insert a state using default values.
     * If the state already exists, nothing will happen.
     * @tparam T The type of the state.
     * @return The App object itself.
     */
    template <typename T>
    App& init_state() {
        Command cmd = command();
        cmd.init_resource<State<T>>();
        cmd.init_resource<NextState<T>>();
        m_state_update.push_back(
            std::make_unique<
                System<Resource<State<T>>, Resource<NextState<T>>>>(
                System<Resource<State<T>>, Resource<NextState<T>>>(
                    this, state_update<T>())));
        return *this;
    }

    /*! @brief Run the app in parallel.
     */
    void run();
};

class LoopPlugin : public Plugin {
   public:
    void build(App& app) override { app.enable_loop(); }
};
}  // namespace entity
}  // namespace pixel_engine