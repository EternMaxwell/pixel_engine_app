﻿#pragma once

#include <unordered_set>

#include "pixel_engine/app/command.h"
#include "pixel_engine/app/query.h"
#include "pixel_engine/app/resource.h"

namespace pixel_engine {
    namespace entity {
        class App;

        template <typename Ret>
        class BasicSystem {
           protected:
            App* app;
            bool has_command = false;
            bool has_query = false;
            double avg_time = 0;  // in milliseconds
            std::vector<
                std::tuple<std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                query_types;
            std::vector<const type_info*> resource_types;
            std::vector<const type_info*> resource_const;
            std::vector<const type_info*> event_read_types;
            std::vector<const type_info*> event_write_types;
            std::vector<const type_info*> state_types;
            std::vector<const type_info*> next_state_types;

            template <typename Arg>
            struct info_add {
                static void add(std::vector<const type_info*>& infos) {
                    infos.push_back(&typeid(std::remove_const_t<Arg>));
                }
            };

            template <typename Arg>
            struct const_infos_adder {
                static void add(std::vector<const type_info*>& infos) {
                    if constexpr (std::is_const_v<Arg>) infos.push_back(&typeid(std::remove_const_t<Arg>));
                }
            };

            template <typename Arg>
            struct non_const_infos_adder {
                static void add(std::vector<const type_info*>& infos) {
                    if constexpr (!std::is_const_v<Arg>) infos.push_back(&typeid(Arg));
                }
            };

            template <typename T>
            struct infos_adder {};

            template <typename... Includes, typename... Withs, typename... Excludes>
            struct infos_adder<Query<Get<Includes...>, With<Withs...>, Without<Excludes...>>> {
                static void add(
                    std::vector<std::tuple<
                        std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                        query_types,
                    std::vector<const type_info*>& resource_types, std::vector<const type_info*>& resource_const,
                    std::vector<const type_info*>& event_read_types, std::vector<const type_info*>& event_write_types,
                    std::vector<const type_info*>& state_types, std::vector<const type_info*>& next_state_types) {
                    std::vector<const type_info*> query_include_types, query_exclude_types, query_include_const;
                    (non_const_infos_adder<Includes>::add(query_include_types), ...);
                    (const_infos_adder<Includes>::add(query_include_const), ...);
                    (info_add<Withs>::add(query_include_types), ...);
                    (info_add<Excludes>::add(query_exclude_types), ...);
                    query_types.push_back(
                        std::make_tuple(query_include_types, query_include_const, query_exclude_types));
                }
            };

            template <typename T>
            struct infos_adder<Resource<T>> {
                static void add(
                    std::vector<std::tuple<
                        std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                        query_types,
                    std::vector<const type_info*>& resource_types, std::vector<const type_info*>& resource_const,
                    std::vector<const type_info*>& event_read_types, std::vector<const type_info*>& event_write_types,
                    std::vector<const type_info*>& state_types, std::vector<const type_info*>& next_state_types) {
                    if constexpr (std::is_const_v<T>)
                        info_add<T>().add(resource_const);
                    else
                        info_add<T>().add(resource_types);
                }
            };

            template <typename T>
            struct infos_adder<EventReader<T>> {
                static void add(
                    std::vector<std::tuple<
                        std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                        query_types,
                    std::vector<const type_info*>& resource_types, std::vector<const type_info*>& resource_const,
                    std::vector<const type_info*>& event_read_types, std::vector<const type_info*>& event_write_types,
                    std::vector<const type_info*>& state_types, std::vector<const type_info*>& next_state_types) {
                    info_add<T>().add(event_read_types);
                }
            };

            template <typename T>
            struct infos_adder<EventWriter<T>> {
                static void add(
                    std::vector<std::tuple<
                        std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                        query_types,
                    std::vector<const type_info*>& resource_types, std::vector<const type_info*>& resource_const,
                    std::vector<const type_info*>& event_read_types, std::vector<const type_info*>& event_write_types,
                    std::vector<const type_info*>& state_types, std::vector<const type_info*>& next_state_types) {
                    info_add<T>().add(event_write_types);
                }
            };

            template <typename T>
            struct infos_adder<State<T>> {
                static void add(
                    std::vector<std::tuple<
                        std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                        query_types,
                    std::vector<const type_info*>& resource_types, std::vector<const type_info*>& resource_const,
                    std::vector<const type_info*>& event_read_types, std::vector<const type_info*>& event_write_types,
                    std::vector<const type_info*>& state_types, std::vector<const type_info*>& next_state_types) {
                    info_add<T>().add(state_types);
                }
            };

            template <typename T>
            struct infos_adder<NextState<T>> {
                static void add(
                    std::vector<std::tuple<
                        std::vector<const type_info*>, std::vector<const type_info*>, std::vector<const type_info*>>>
                        query_types,
                    std::vector<const type_info*>& resource_types, std::vector<const type_info*>& resource_const,
                    std::vector<const type_info*>& event_read_types, std::vector<const type_info*>& event_write_types,
                    std::vector<const type_info*>& state_types, std::vector<const type_info*>& next_state_types) {
                    info_add<T>().add(next_state_types);
                }
            };

            template <typename Arg, typename... Args>
            void add_infos_inernal() {
                if constexpr (std::is_same_v<Arg, Command>) {
                    has_command = true;
                } else if constexpr (is_template_of<Query, Arg>::value) {
                    has_query = true;
                    infos_adder<Arg>().add(
                        query_types, resource_types, resource_const, event_read_types, event_write_types, state_types,
                        next_state_types);
                } else {
                    infos_adder<Arg>().add(
                        query_types, resource_types, resource_const, event_read_types, event_write_types, state_types,
                        next_state_types);
                }
                if constexpr (sizeof...(Args) > 0) add_infos<Args...>();
            }

            template <typename... Args>
            void add_infos() {
                if constexpr (sizeof...(Args) > 0) add_infos_inernal<Args...>();
            }

           public:
            BasicSystem(App* app) : app(app) {}
            bool contrary_to(std::shared_ptr<BasicSystem>& other) {
                if (has_command && (other->has_command || other->has_query)) return true;
                if (has_query && (other->has_command || other->has_query)) return true;
                bool query_contrary = false;
                for (auto& [query_include_types, query_include_const, query_exclude_types] : query_types) {
                    for (auto& [other_query_include_types, other_query_include_const, other_query_exclude_types] :
                         other->query_types) {
                        for (auto type : query_include_types) {
                            if (std::find(other_query_include_types.begin(), other_query_include_types.end(), type) !=
                                other_query_include_types.end()) {
                                query_contrary = true;
                                break;
                            }
                            if (std::find(other_query_include_const.begin(), other_query_include_const.end(), type) !=
                                other_query_exclude_types.end()) {
                                query_contrary = true;
                                break;
                            }
                        }
                        if (!query_contrary) {
                            for (auto type : other_query_include_types) {
                                if (std::find(query_include_types.begin(), query_include_types.end(), type) !=
                                    query_include_types.end()) {
                                    query_contrary = true;
                                    break;
                                }
                                if (std::find(query_include_const.begin(), query_include_const.end(), type) !=
                                    query_exclude_types.end()) {
                                    query_contrary = true;
                                    break;
                                }
                            }
                        }
                        if (query_contrary) {
                            for (auto type : query_exclude_types) {
                                if (std::find(
                                        other_query_include_types.begin(), other_query_include_types.end(), type) !=
                                    other_query_include_types.end()) {
                                    query_contrary = false;
                                    break;
                                }
                                if (std::find(
                                        other_query_include_const.begin(), other_query_include_const.end(), type) !=
                                    other_query_include_const.end()) {
                                    query_contrary = false;
                                    break;
                                }
                            }
                        }
                        if (query_contrary) {
                            for (auto type : other_query_exclude_types) {
                                if (std::find(query_include_types.begin(), query_include_types.end(), type) !=
                                    query_include_types.end()) {
                                    query_contrary = false;
                                    break;
                                }
                                if (std::find(query_include_const.begin(), query_include_const.end(), type) !=
                                    query_include_const.end()) {
                                    query_contrary = false;
                                    break;
                                }
                            }
                        }
                    }
                }

                bool resource_one_empty = resource_types.empty() || other->resource_types.empty();
                bool resource_contrary = !resource_one_empty;
                if (!resource_one_empty) {
                    for (auto type : resource_types) {
                        if (std::find(other->resource_const.begin(), other->resource_const.end(), type) !=
                            other->resource_const.end()) {
                            resource_contrary = true;
                        }
                        if (std::find(other->resource_types.begin(), other->resource_types.end(), type) !=
                            other->resource_types.end()) {
                            resource_contrary = true;
                        }
                    }
                    for (auto type : other->resource_types) {
                        if (std::find(resource_const.begin(), resource_const.end(), type) != resource_const.end()) {
                            resource_contrary = true;
                        }
                        if (std::find(resource_types.begin(), resource_types.end(), type) != resource_types.end()) {
                            resource_contrary = true;
                        }
                    }
                }
                bool event_contrary = false;
                for (auto type : event_write_types) {
                    if (std::find(other->event_write_types.begin(), other->event_write_types.end(), type) !=
                        other->event_write_types.end()) {
                        event_contrary = true;
                    }
                    if (std::find(other->event_read_types.begin(), other->event_read_types.end(), type) !=
                        other->event_read_types.end()) {
                        event_contrary = true;
                    }
                }
                for (auto type : other->event_write_types) {
                    if (std::find(event_write_types.begin(), event_write_types.end(), type) !=
                        event_write_types.end()) {
                        event_contrary = true;
                    }
                    if (std::find(event_read_types.begin(), event_read_types.end(), type) != event_read_types.end()) {
                        event_contrary = true;
                    }
                }
                bool state_contrary = false;
                for (auto type : next_state_types) {
                    if (std::find(other->next_state_types.begin(), other->next_state_types.end(), type) !=
                        other->next_state_types.end()) {
                        state_contrary = true;
                    }
                }
                for (auto type : other->next_state_types) {
                    if (std::find(next_state_types.begin(), next_state_types.end(), type) != next_state_types.end()) {
                        state_contrary = true;
                    }
                }

                return query_contrary || resource_contrary || event_contrary || state_contrary;
            }
            void print_info_types_name() {
                std::cout << "has command: " << has_command << std::endl;

                for (auto& [query_include_types, query_include_const, query_exclude_types] : query_types) {
                    std::cout << "query_include_types: ";
                    for (auto type : query_include_types) std::cout << type->name() << " ";
                    std::cout << std::endl;

                    std::cout << "query_include_const: ";
                    for (auto type : query_include_const) std::cout << type->name() << " ";
                    std::cout << std::endl;

                    std::cout << "query_exclude_types: ";
                    for (auto type : query_exclude_types) std::cout << type->name() << " ";
                    std::cout << std::endl;
                }

                std::cout << "resource_types: ";
                for (auto type : resource_types) std::cout << type->name() << " ";
                std::cout << std::endl;

                std::cout << "resource_const: ";
                for (auto type : resource_const) std::cout << type->name() << " ";
                std::cout << std::endl;

                std::cout << "event_read_types: ";
                for (auto type : event_read_types) std::cout << type->name() << " ";
                std::cout << std::endl;

                std::cout << "event_write_types: ";
                for (auto type : event_write_types) std::cout << type->name() << " ";
                std::cout << std::endl;

                std::cout << "state_types: ";
                for (auto type : state_types) std::cout << type->name() << " ";
                std::cout << std::endl;

                std::cout << "next_state_types: ";
                for (auto type : next_state_types) std::cout << type->name() << " ";
                std::cout << std::endl;
            }
            const double get_avg_time() { return avg_time; }
            virtual Ret run() = 0;
            virtual const char* func_name() { return "unknown"; }
        };

        template <typename... Args>
        class System : public BasicSystem<void> {
           private:
            std::function<void(Args...)> func;

           public:
            System(App* app, void (*func)(Args...)) : BasicSystem(app), func(func) { add_infos<Args...>(); }
            System(App* app, std::function<void(Args...)> func) : BasicSystem(app), func(func) { add_infos<Args...>(); }
            void run() {
                auto start_time = std::chrono::high_resolution_clock::now();
                app->run_system_v(func);
                auto end_time = std::chrono::high_resolution_clock::now();
                auto delta =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count() / 1000000.0;
                avg_time = delta * 0.1 + avg_time * 0.9;
            }
            const char* func_name() override { return typeid(func).name(); }
        };

        struct condition {
           public:
            bool if_run(App* app) { return true; }
        };

        template <typename T>
        struct condition_state : public condition {
           private:
            T m_state;
            App* m_app;
            std::function<bool(Resource<State<T>>)> m_func = [&](Resource<State<T>> state) {
                return state->is_state(m_state);
            };

           public:
            condition_state(T state) : m_state(state) {}
            bool if_run(App* app) override {
                m_app = app;
                return m_app->run_system_v(m_func);
            }
        };
    }  // namespace entity
}  // namespace pixel_engine