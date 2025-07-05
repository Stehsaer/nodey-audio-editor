#pragma once

#include <any>
#include <format>
#include <functional>
#include <json/json.h>
#include <memory>
#include <optional>
#include <set>
#include <source_location>
#include <stop_token>

#include "utility/logic-error-utility.hpp"

namespace infra
{
	using Id_t = int;

	template <typename T, typename Ty>
	concept Has_static_processor_info_func = requires {
		{ T::get_processor_info() } -> std::same_as<Ty>;
	};

	// 处理器基类
	// - 每一个新的处理器都需要继承这个基类并实现所有的virtual函数
	class Processor
	{
	  public:

		// 处理器的产品基类
		// - 每一个新的产品种类都需要继承这个基类
		class Product
		{
		  public:

			Product() = default;
			virtual ~Product() = default;
			const std::type_info& get_typeinfo() const { return typeid(*this); }
		};

		// 描述处理器的输入/输出端口属性（元数据）
		struct Pin_attribute
		{
			std::string identifier;                                   // 标识符
			std::string display_name;                                 // 显示名称
			std::reference_wrapper<const std::type_info> type;        // 产品的类型信息
			bool is_input;                                            // 是否为输入端口
			std::function<std::shared_ptr<Product>()> generate_func;  // 生成产品的函数
		};

		// 描述处理器的基本信息（元数据）
		struct Info
		{
			std::string identifier;    // 标识符
			std::string display_name;  // 显示名称
			bool singleton = false;    // 是否为单例处理器（每一个图中至多一个实例）
			std::function<std::unique_ptr<Processor>()> generate;  // 生成处理器实例的函数
			std::string description;                               // 描述信息
		};

		// 包含简述、解释和详细信息的运行时错误
		// - 用于处理器在运行时抛出的错误
		// - 供UI捕捉并显示用户友好的报错信息
		struct Runtime_error : public std::runtime_error
		{
			std::string message, explanation, detail;

			Runtime_error(std::string message, std::string explanation, std::string detail = "") :
				std::runtime_error(
					std::format("{} (Detail: {}) (Explanation: {})", message, detail, explanation)
				),
				message(std::move(message)),
				explanation(std::move(explanation)),
				detail(std::move(detail))
			{
			}
		};

		// 存储所有处理器类的属性
		static std::map<std::string, Processor::Info> processor_map;

		Processor() = default;
		virtual ~Processor() = default;

		// 返回处理模块的输入输出端口属性
		virtual std::vector<Processor::Pin_attribute> get_pin_attributes() const = 0;

		// 获取处理器的元数据
		// - 注意，基类中需要实现一个静态的get_processor_info()函数供注册时使用
		virtual Processor::Info get_processor_info_non_static() const = 0;

		// 将模块设置/信息导出为JSON
		virtual Json::Value serialize() const = 0;

		// 从serialize()导出的JSON中恢复得到信息
		virtual void deserialize(const Json::Value& value) = 0;

		// 绘制UI节点标题
		virtual void draw_title() = 0;

		// 绘制UI节点内容
		// - 若属性被修改，则返回true，否则返回false
		// - 注意：这个接口后续可能还有大改动
		virtual bool draw_content(bool readonly) = 0;

		// 处理货物
		// - 由Runner调用，处理器需要实现这个函数来处理输入的产品并生成输出产品
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		) = 0;

		// 静态的注册函数
		template <typename T>
			requires(std::is_base_of_v<Processor, T> && Has_static_processor_info_func<T, Processor::Info>)
		static void register_processor()
		{
			const Info processor_info = T::get_processor_info();

			if (processor_map.contains(processor_info.identifier))
				THROW_LOGIC_ERROR(
					"Processor with identifier '{}' already registered",
					processor_info.identifier
				)

			processor_map[processor_info.identifier] = T::get_processor_info();
		}
	};

	// 解析输入映射，获取指定键对应的输入产品
	// - 获取失败时，返回std::nullopt，且注意需要检查返回结果是否有效；若无效，则需要抛出异常通知UI
	template <typename T>
	std::optional<std::reference_wrapper<T>> get_input_item(
		const std::map<std::string, std::shared_ptr<Processor::Product>>& input,
		const std::string& key
	)
	{
		const auto find = input.find(key);

		if (find == input.end()) return std::nullopt;

		if (find->second == nullptr) THROW_LOGIC_ERROR("Found nullptr in input map for key '{}'", key);

		if (find->second->get_typeinfo() != typeid(T))
			THROW_LOGIC_ERROR(
				"Type mismatch in input map for key '{}', expected {}, got {}",
				key,
				typeid(T).name(),
				find->second->get_typeinfo().name()
			);

		return *std::dynamic_pointer_cast<T>(find->second);
	}

	// 解析输出映射，获取指定键对应的输出产品集合
	template <typename T>
	std::set<std::shared_ptr<T>> get_output_item(
		const std::map<std::string, std::set<std::shared_ptr<Processor::Product>>>& output,
		const std::string& key
	)
	{
		const auto find = output.find(key);

		if (find == output.end()) THROW_LOGIC_ERROR("Key '{}' not found in output map", key);

		std::set<std::shared_ptr<T>> output_set;
		for (auto& item : find->second)
		{
			if (item == nullptr) THROW_LOGIC_ERROR("Found nullptr in output map for key '{}'", key);
			output_set.emplace(std::dynamic_pointer_cast<T>(item));
		}

		return output_set;
	}

	// 全局函数，注册所有处理器
	// - 每次新增处理器时，都需要在注册函数中添加
	// - 注册函数位于`src/register.cpp`
	void register_all_processors();
}