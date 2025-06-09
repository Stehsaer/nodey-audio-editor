#include "infra/runner.hpp"

#include <barrier>
#include <boost/fiber/algo/work_stealing.hpp>
#include <boost/fiber/operations.hpp>

#include <print>

namespace infra
{
	void Runner::generate_processor_resources(const Graph& graph)
	{
		graph.check_graph();

		for (const auto& [id, node] : graph.nodes)
		{
			std::unique_ptr<Processor_resource> resource = std::make_unique<Processor_resource>();

			resource->processor = node.processor;
			processor_resources.emplace(id, std::move(resource));

			for (auto pin_id : node.pins)
			{
				const auto& info = graph.pins.at(pin_id).attribute;
				if (!info.is_input)
				{
					processor_resources.at(id)->output_payloads.emplace(
						info.identifier,
						std::set<std::shared_ptr<Processor::Product>>{}
					);
				}
			}
		}

		for (const auto& [idx, link] : graph.links)
		{
			const auto [from, to] = link;
			const auto &from_attribute = graph.pins.at(from), &to_attribute = graph.pins.at(to);

			std::shared_ptr<Processor::Product> product = from_attribute.attribute.generate_func();

			processor_resources.at(from_attribute.parent)
				->output_payloads[from_attribute.attribute.identifier]
				.emplace(product);

			processor_resources.at(to_attribute.parent)
				->input_payloads.emplace(to_attribute.attribute.identifier, product);

			link_products.emplace(idx, product);
		}
	}

	Runner::~Runner()
	{
		for (auto& [_, resource] : processor_resources)
		{
			resource->stop_source = true;
			if (resource->fiber.joinable())
				resource->fiber.join();
			else
				while (resource->state == State::Running) std::this_thread::yield();
		}
	}

	void Runner::launch_threads()
	{
		for (auto& [idx, resource] : processor_resources)
		{
			resource->fiber = boost::fibers::fiber(
				[this, ptr = resource.get(), idx]
				{
					const auto find_node_data = node_data.find(idx);
					std::any fallback;

					try
					{
						ptr->state = State::Running;
						ptr->processor->process_payload(
							ptr->input_payloads,
							ptr->output_payloads,
							ptr->stop_source,
							find_node_data == node_data.end() ? fallback : *(find_node_data->second)
						);

						ptr->state = State::Finished;
					}
					catch (const Processor::Runtime_error& e)
					{
						ptr->exception = e;
						ptr->state = State::Error;
					}
					catch (const std::runtime_error& e)
					{
						ptr->exception = e;
						ptr->state = State::Error;
					}
					catch (const std::bad_any_cast& e)
					{
						ptr->exception = std::logic_error(
							std::format(
								"Bad any cast found in the processor \"{}\"",
								ptr->processor->get_processor_info_non_static().identifier
							)
						);
						ptr->state = State::Error;
					}
					catch (const std::bad_alloc& e)
					{
						ptr->exception = std::runtime_error(
							std::format(
								"Memory allocation failed in the processor \"{}\"",
								ptr->processor->get_processor_info_non_static().identifier
							)
						);
						ptr->state = State::Error;
					}
					catch (const std::bad_optional_access& e)
					{
						ptr->exception = std::logic_error(
							std::format(
								"Bad optional access found in the processor \"{}\"",
								ptr->processor->get_processor_info_non_static().identifier
							)
						);
						ptr->state = State::Error;
					}
					catch (const std::logic_error& e)
					{
						ptr->exception = e;
						ptr->state = State::Error;
					}
					catch (...)
					{
						ptr->exception = std::exception();
						ptr->state = State::Error;
					}
				}
			);
		}
	}

	std::unique_ptr<Runner> Runner::create_and_run(
		const Graph& graph,
		std::map<Id_t, std::shared_ptr<std::any>> node_data
	)
	{
		auto runner = std::make_unique<Runner>();
		runner->node_data = std::move(node_data);

		runner->generate_processor_resources(graph);
		std::thread(&Runner::launch_threads, runner.get()).detach();

		return runner;
	}
}