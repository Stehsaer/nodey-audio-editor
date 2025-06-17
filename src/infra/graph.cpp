#include "infra/graph.hpp"
#include "utility/logic-error-utility.hpp"

#include <algorithm>
#include <stack>

namespace infra
{
	Id_t Graph::add_node(std::unique_ptr<Processor> processor)
	{
		Id_t id = find_empty(nodes);
		const auto info = processor->get_processor_info_non_static();

		nodes[id] = {.processor = std::move(processor), .pins = std::set<Id_t>()};
		update_node_pin(id);

		if (info.singleton) singleton_node_map.emplace(info.identifier, id);

		return id;
	}

	void Graph::remove_node(Id_t id)
	{
		auto& item = nodes[id];
		auto& set = item.pins;
		const auto info = item.processor->get_processor_info_non_static();
		if (info.singleton)
		{
			auto it = singleton_node_map.find(info.identifier);

			if (it == singleton_node_map.end()) THROW_LOGIC_ERROR("Singleton node ID not found");
			if (it->second != id)
				THROW_LOGIC_ERROR("Singleton node ID mismatch, expected {}, got {}", it->second, id);

			singleton_node_map.erase(it);
		}

		for (const auto& id : set) pins.erase(id);
		std::erase_if(
			links,
			[&set](auto pair) -> bool
			{ return set.contains(pair.second.from) || set.contains(pair.second.to); }
		);
		set.clear();

		nodes.erase(id);
	}

	void Graph::update_node_pin(Id_t id)
	{
		auto& item = nodes[id];

		auto& node = item.processor;
		auto& set = item.pins;

		// 清除旧引脚
		for (const auto& id : set) pins.erase(id);
		std::erase_if(
			links,
			[&set](auto pair) -> bool
			{ return set.contains(pair.second.from) || set.contains(pair.second.to); }
		);
		set.clear();

		// 重新添加含有新属性的引脚
		const auto attributes = node->get_pin_attributes();
		for (const auto& attribute : attributes)
		{
			const auto pin_id = find_empty(pins);
			set.emplace(pin_id);
			pins.erase(pin_id);
			pins.emplace(pin_id, Pin{.parent = id, .attribute = attribute});
		}
	}

	Id_t Graph::add_link(Id_t from, Id_t to)
	{
		if (!check_node_type_match(from, to)) [[unlikely]]
			throw Mismatched_pin_error{from, to};
		if (!check_multiple_input(to)) [[unlikely]]
			throw Multiple_input_error{to};

		const auto id = find_empty(links);
		links.erase(id);
		links.emplace(id, Link{.from = from, .to = to});
		return id;
	}

	void Graph::remove_link(Id_t id)
	{
		links.erase(id);
	}

	void Graph::remove_link(Id_t from, Id_t to)
	{
		std::erase_if(
			links,
			[&](const auto& item) -> bool { return item.second.from == from && item.second.to == to; }
		);
	}

	std::map<Id_t, Id_t> Graph::get_pin_to_node_map() const
	{
		std::map<Id_t, Id_t> pin_to_node;
		for (const auto& [idx, item] : nodes)
		{
			const auto& [node, pins] = std::tie(item.processor, item.pins);
			for (auto pin : pins) pin_to_node[pin] = idx;
		}

		return pin_to_node;
	}

	std::map<Id_t, std::set<Id_t>> Graph::get_node_input_map() const
	{
		std::map<Id_t, std::set<Id_t>> map;
		const auto pin_to_node = get_pin_to_node_map();

		for (const auto& [idx, _] : nodes)
		{
			auto count_view
				= links | std::views::values
				| std::views::filter([idx, this](Link link) { return pins.at(link.to).parent == idx; })
				| std::views::transform(Link::Transform::from);

			map.emplace(idx, std::set<Id_t>(count_view.begin(), count_view.end()));
		}

		return map;
	}

	void Graph::check_graph() const
	{
		std::map<Id_t, Id_t> pin_to_node = get_pin_to_node_map();
		std::map<Id_t, std::set<Id_t>> node_to_output_map;
		std::map<Id_t, std::set<Id_t>> node_to_input_map = get_node_input_map();
		std::map<Id_t, size_t> node_in_degree;
		std::set<Id_t> zero_degree_set;

		for (const auto& [idx, input_set] : node_to_input_map) node_in_degree.emplace(idx, input_set.size());

		// 统计节点关系和入度
		for (const auto [idx, item] : links)
		{
			const auto [from, to] = item;

			if (!check_node_type_match(from, to)) throw Mismatched_pin_error{from, to};
			if (!check_multiple_input(to)) [[unlikely]]
				throw Multiple_input_error(to);

			node_to_output_map[pins.at(from).parent].insert(pins.at(to).parent);
		}

		/* 寻找0入度的节点 */
		{
			auto zero_degree_nodes = node_in_degree
								   | std::views::filter([](const auto& item) { return item.second == 0; })
								   | std::views::keys;

			zero_degree_set.insert_range(zero_degree_nodes);

			if (!nodes.empty() && zero_degree_set.empty()) [[unlikely]]
				throw Loop_detected_error{};
		}

		/* 使用状态机遍历 */

		enum class State
		{
			Check_node,
			Push_node,
			Traverse_children,
			Pop_node
		};

		struct Traverse_stack_element
		{
			std::set<Id_t>::const_iterator node, begin, end;
		};

		State state = State::Traverse_children;
		std::vector<Traverse_stack_element> traverse_stack = {
			Traverse_stack_element{.node = {}, .begin = zero_degree_set.begin(), .end = zero_degree_set.end()}
		};
		std::set<Id_t>::const_iterator node;
		std::set<Id_t> visited_nodes;

		auto check_node = [&](std::set<Id_t>::const_iterator it) -> bool
		{
			return std::ranges::all_of(traverse_stack, [&](const auto& elem) { return it != elem.node; });
		};

		while (!traverse_stack.empty())
		{
			switch (state)
			{
			case State::Check_node:
				if (!check_node(node)) throw Loop_detected_error{};
				visited_nodes.insert(*node);
				state = State::Push_node;
				break;

			case State::Push_node:
				traverse_stack.push_back(
					Traverse_stack_element{
						.node = node,
						.begin = node_to_output_map[*node].begin(),
						.end = node_to_output_map[*node].end()
					}
				);
				state = State::Traverse_children;
				break;

			case State::Traverse_children:
				if (traverse_stack.back().begin == traverse_stack.back().end)
				{
					state = State::Pop_node;
					break;
				}
				node = traverse_stack.back().begin++;
				state = State::Check_node;
				break;

			case State::Pop_node:
				traverse_stack.pop_back();
				state = State::Traverse_children;
				break;
			}
		}

		for (auto node : visited_nodes) node_in_degree.erase(node);
		if (!node_in_degree.empty()) [[unlikely]]
			throw Loop_detected_error{};
	}
}