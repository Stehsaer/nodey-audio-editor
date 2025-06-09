#pragma once

#include "graph.hpp"

#include <boost/fiber/condition_variable.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/mutex.hpp>

#include <any>
#include <future>
#include <mutex>
#include <thread>

namespace infra
{
	// 处理器调度器
	// - 负责管理处理器的执行和资源分配;
	// - 使用boost::fibers实现协程调度;
	// [TODO] 实现多线程，提高CPU利用率
	class Runner
	{
	  public:

		// 每个处理器工作纤程的状态
		enum class State
		{
			Ready,     // 等待被执行
			Running,   // 执行中
			Finished,  // 处理完成
			Error      // 处理出现错误
		};

	  private:

		// 聚合了处理器资源的struct
		struct Processor_resource
		{
			/* 资源 */

			std::shared_ptr<Processor> processor;                                       // 处理器本体
			std::map<std::string, std::shared_ptr<Processor::Product>> input_payloads;  // 输入产品
			std::map<std::string, std::set<std::shared_ptr<Processor::Product>>> output_payloads;  // 输出产品

			/* 调度管理 */

			boost::fibers::fiber fiber;               // 纤程对象
			std::atomic<bool> stop_source;            // 停止信号源
			std::atomic<State> state = State::Ready;  // 执行状态
			std::any exception;                       // 执行中抛出的错误
		};

		std::map<Id_t, std::shared_ptr<Processor_resource>> processor_resources;
		std::map<Id_t, std::shared_ptr<Processor::Product>> link_products;  // 追踪每一个连结对应的产品实例
		std::map<Id_t, std::shared_ptr<std::any>> node_data;  // 存储节点对应的用户数据（由UI给出）

		// 生成处理器资源
		void generate_processor_resources(const Graph& graph);

		// 启动纤程的内核线程
		void launch_threads();

	  public:

		Runner() = default;

		Runner(const Runner&) = delete;
		Runner(Runner&&) = delete;
		Runner& operator=(const Runner&) = delete;
		Runner& operator=(Runner&&) = delete;

		~Runner();

		// 根据图和用户数据，创建新的Runner实例并马上返回
		static std::unique_ptr<Runner> create_and_run(
			const Graph& graph,
			std::map<Id_t, std::shared_ptr<std::any>> node_data
		);

		// 获取处理器资源集合，可用于检测执行状态细节
		const auto& get_processor_resources() const { return processor_resources; }

		// 获取连结对应的产品实例，可用于检测执行状态细节
		const auto& get_link_products() const { return link_products; }
	};
}