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

		nodes[id] = {.processor = std::move(processor), .pins = std::set<Id_t>(), .pin_name_map = {}};
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

		std::map<std::string, Id_t> prev_input_link;
		std::map<std::string, std::set<Id_t>> prev_output_link;

		// 清除旧引脚并记录原来的链接
		for (auto it = links.begin(); it != links.end();)
		{
			const auto [from, to] = it->second;

			if (set.contains(from))
			{
				prev_output_link[pins.at(from).attribute.identifier].emplace(to);
				it = links.erase(it);
			}
			else if (set.contains(to))
			{
				prev_input_link[pins.at(to).attribute.identifier] = from;
				it = links.erase(it);
			}
			else
				++it;
		}

		for (const auto& id : set) pins.erase(id);

		set.clear();
		item.pin_name_map.clear();

		// 重新添加含有新属性的引脚
		const auto attributes = node->get_pin_attributes();
		for (const auto& attribute : attributes)
		{
			const auto pin_id = find_empty(pins);
			set.emplace(pin_id);
			pins.emplace(pin_id, Pin{.parent = id, .attribute = attribute});

			if (auto find_prev_input = prev_input_link.find(attribute.identifier);
				find_prev_input != prev_input_link.end()
				&& attribute.type.get() == pins.at(find_prev_input->second).attribute.type.get())
				links.emplace(find_empty(links), Link{.from = find_prev_input->second, .to = pin_id});

			if (auto find_prev_output = prev_output_link.find(attribute.identifier);
				find_prev_output != prev_output_link.end())
				for (const auto& prev_to : find_prev_output->second)
				{
					if (attribute.type.get() == pins.at(prev_to).attribute.type.get())
						links.emplace(find_empty(links), Link{.from = pin_id, .to = prev_to});
				}

			if (item.pin_name_map.contains(attribute.identifier))
				THROW_LOGIC_ERROR("Pin name {} already exists for node ID {}", attribute.identifier, id);
			item.pin_name_map.emplace(attribute.identifier, pin_id);
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

	Json::Value Graph::serialize() const
	{
		Json::Value node_json(Json::ValueType::objectValue);

		// 节点格式 <node>
		// "0": {
		//     "identifier": "node_identifier",
		//     "info": { ... },  // 节点信息
		//     "position": { // 节点位置
		//         "x": 0,
		//         "y": 0
		//     }
		// }

		for (const auto& [id, node] : nodes)
		{
			const auto processor_info = node.processor->get_processor_info_non_static();

			Json::Value item;

			item["identifier"] = processor_info.identifier;
			item["info"] = node.processor->serialize();
			item["position"]["x"] = node.position.x;
			item["position"]["y"] = node.position.y;

			node_json[std::to_string(id)] = std::move(item);
		}

		Json::Value link_json(Json::ValueType::arrayValue);

		// 连结格式 <link>
		// {
		//     "from": {
		//         "node": 0,
		//         "pin": "from_pin_name"
		//     },
		//     "to": {
		//         "node": 1,
		//         "pin": "to_pin_name"
		//     }
		// }

		for (const auto& [idx, link] : links)
		{
			const auto [from, to] = link;
			const auto& from_pin = pins.at(from);
			const auto& to_pin = pins.at(to);

			const auto& from_name = from_pin.attribute.identifier;
			const auto& to_name = to_pin.attribute.identifier;

			Json::Value from_json;
			from_json["node"] = from_pin.parent;
			from_json["pin"] = from_name;

			Json::Value to_json;
			to_json["node"] = to_pin.parent;
			to_json["pin"] = to_name;

			Json::Value item;
			item["from"] = std::move(from_json);
			item["to"] = std::move(to_json);
			link_json.append(std::move(item));
		}

		// 最终输出格式
		// {
		//     "nodes": {
		//         "0": { ... },  // 节点信息
		// 	       "1": { ... },
		//         ...
		//     },
		//     "links": [
		//         {
		//             "from": ...,
		// 		       "to": ...
		//         },
		//         {
		//             ...
		//         }
		//     ]
		// }

		Json::Value result;
		result["nodes"] = std::move(node_json);
		result["links"] = std::move(link_json);

		return result;
	}

	Graph Graph::deserialize(const Json::Value& value)
	try
	{
		if (!value.isObject()) throw Invalid_file_error("Invalid graph format, expected object");

		const auto& nodes_json = value["nodes"];
		const auto& links_json = value["links"];

		if (!nodes_json.isObject()) throw Invalid_file_error("Invalid nodes format, expected object");
		if (!links_json.isArray()) throw Invalid_file_error("Invalid links format, expected array");

		Graph graph;

		for (const auto& key : nodes_json.getMemberNames())
		{
			// id转int
			size_t integer_processed;
			const Id_t id = std::stoi(key, &integer_processed);
			if (integer_processed != key.length())
				throw Invalid_file_error(std::format("Invalid node ID: {}", key));

			const auto& node_json = nodes_json[key];
			if (!node_json.isObject())
				throw Invalid_file_error(std::format("Invalid node JSON format for ID: {}", id));

			const std::string identifier = node_json["identifier"].asString();

			// 查找对应的节点元信息
			const auto find_metadata = Processor::processor_map.find(identifier);
			if (find_metadata == Processor::processor_map.end())
				throw Invalid_file_error(std::format("Unknown processor identifier: {}", identifier));
			const auto& metadata = find_metadata->second;

			std::shared_ptr<Processor> processor = metadata.generate();
			processor->deserialize(node_json["info"]);

			// 单例处理逻辑
			if (metadata.singleton)
			{
				if (graph.singleton_node_map.contains(identifier))
					throw Invalid_file_error(std::format("Duplicating singleton node \"{}\"", identifier));

				graph.singleton_node_map.emplace(identifier, id);
			}

			graph.nodes.emplace(
				id,
				Graph::Node{
					.processor = std::move(processor),
					.pins = std::set<Id_t>(),
					.pin_name_map = {},
					.position
					= ImVec2(node_json["position"]["x"].asFloat(), node_json["position"]["y"].asFloat())
				}
			);

			graph.update_node_pin(id);
		}

		for (const auto& link : links_json)
		{
			if (!link.isObject()) throw Invalid_file_error("Invalid link JSON format, expected object");

			const auto& from_json = link["from"];
			const auto& to_json = link["to"];

			if (!from_json.isObject() || !to_json.isObject())
				throw Invalid_file_error("Invalid link 'from' or 'to' JSON format, expected object");

			const Id_t from_node = from_json["node"].asInt();
			const Id_t to_node = to_json["node"].asInt();

			const std::string from_pin_name = from_json["pin"].asString();
			const std::string to_pin_name = to_json["pin"].asString();

			if (!graph.nodes.contains(from_node) || !graph.nodes.contains(to_node))
				throw Invalid_file_error(
					std::format("Link references non-existent node: {} -> {}", from_node, to_node)
				);

			const auto& from_pin_map = graph.nodes.at(from_node).pin_name_map;
			const auto& to_pin_map = graph.nodes.at(to_node).pin_name_map;

			if (!from_pin_map.contains(from_pin_name) || !to_pin_map.contains(to_pin_name))
				throw Invalid_file_error(
					std::format(
						"Link references non-existent pin: {}.{} -> {}.{}",
						from_node,
						from_pin_name,
						to_node,
						to_pin_name
					)
				);

			const Id_t from_id = from_pin_map.at(from_pin_name);
			const Id_t to_id = to_pin_map.at(to_pin_name);

			graph.add_link(from_id, to_id);
		}

		return graph;
	}
	catch (const Json::Exception& e)
	{
		throw Invalid_file_error(std::format("Failed to deserialize graph due to JSON error: {}", e.what()));
	}
}