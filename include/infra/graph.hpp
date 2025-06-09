// graph.hpp
// 定义图类，提供增删节点操作和一些算法

#pragma once

#include "processor.hpp"

#include <algorithm>
#include <imgui.h>
#include <memory>
#include <ranges>
#include <set>
#include <tuple>
#include <vector>

namespace infra
{

	// 节点图
	// - 管理节点、引脚和连结，负责维护图的结构
	class Graph
	{
	  public:

		struct Node
		{
			std::shared_ptr<Processor> processor;  // 节点对应的处理器
			std::set<Id_t> pins;                   // 节点拥有的引脚ID
			ImVec2 position = ImVec2(0, 0);        // 节点在编辑器的位置
		};

		struct Pin
		{
			Id_t parent;                         // 引脚对应的节点ID
			Processor::Pin_attribute attribute;  // 引脚属性
		};

		struct Link
		{
			Id_t from, to;

			bool operator==(const Link& other) const
			{
				return std::tie(from, to) == std::tie(other.from, other.to);
			}

			struct Transform
			{
				static Id_t to(const Link& link) { return link.to; }
				static Id_t from(const Link& link) { return link.from; }
			};
		};

		std::map<Id_t, Node> nodes;                      // 节点
		std::map<Id_t, Pin> pins;                        // 节点的引脚
		std::map<Id_t, Link> links;                      // 连结
		std::map<std::string, Id_t> singleton_node_map;  // 存储单例节点的映射

	  private:

		// 在map中寻找第一个空闲的ID
		template <typename T>
		static Id_t find_empty(std::map<Id_t, T>& list)
		{
			using Iterator = decltype(list.begin());
			Iterator first = list.begin();

			if (first == list.end()) return 0;
			if (first->first != 0) return 0;

			Iterator second = std::next(list.begin());
			while (second != list.end())
			{
				if (first->first + 1 != second->first) return first->first + 1;
				++first;
				++second;
			}

			return first->first + 1;
		}

	  public:

		/* 增删函数 */

		// 添加新的节点
		Id_t add_node(std::unique_ptr<Processor> processor);

		// 删除节点
		void remove_node(Id_t id);

		// 更新节点信息
		void update_node_pin(Id_t id);

		// 新增连结
		Id_t add_link(Id_t from, Id_t to);

		// 用ID删除连结
		void remove_link(Id_t id);

		// 用From和To引脚ID删除连结
		void remove_link(Id_t from, Id_t to);

		/* 图的辅助函数 */

		std::map<Id_t, Id_t> get_pin_to_node_map() const;
		std::map<Id_t, std::set<Id_t>> get_node_input_map() const;

		/* 检测函数 */

		// 引脚类型不匹配异常
		struct Mismatched_pin_error : public std::runtime_error
		{
			Id_t from, to;  // 不匹配的引脚ID

			Mismatched_pin_error(Id_t from, Id_t to) :
				std::runtime_error(std::format("Mismatch Pin: {}, {}", from, to)),
				from(from),
				to(to)
			{
			}
		};

		// 图中检测到环
		struct Loop_detected_error : public std::runtime_error
		{
			Loop_detected_error() :
				std::runtime_error("Loop Detected")
			{
			}
		};

		// 输入引脚有多个输入
		// - 通常是导入了不合法的图，UI按理说不会出现这种情况
		struct Multiple_input_error : public std::runtime_error
		{
			Id_t pin;  // 出错的引脚

			Multiple_input_error(Id_t pin) :
				std::runtime_error(std::format("Multiple Inputs in Input Pin: {}", pin)),
				pin(pin)
			{
			}
		};

		// 检查图是否有效，若无效则抛出上面对应的异常
		void check_graph() const;

		// 检查两个引脚是否属于同一类型的节点
		bool check_node_type_match(Id_t from, Id_t to) const
		{
			return &pins.at(from).attribute.type.get() == &pins.at(to).attribute.type.get();
		}

		// 检查引脚是否有多个输入
		bool check_multiple_input(Id_t pin_id) const
		{
			size_t count = 0;
			for (const auto& [_, link] : links)
			{
				if (link.to == pin_id) ++count;
				if (count > 1) return false;  // 超过一个输入
			}

			return true;
		}
	};
}