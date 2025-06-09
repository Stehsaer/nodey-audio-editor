// 辅助宏，用于抛出带有源信息的逻辑错误
#define THROW_LOGIC_ERROR(msg, ...)                                                                          \
	{                                                                                                        \
		throw std::logic_error(                                                                              \
			std::format(                                                                                     \
				"{}({})" msg,                                                                                \
				std::source_location::current().file_name(),                                                 \
				std::source_location::current().line(),                                                      \
				##__VA_ARGS__                                                                                \
			)                                                                                                \
		);                                                                                                   \
	}
