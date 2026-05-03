#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>

#include "sherpa-onnx/c-api/c-api.h"

namespace
{
std::string ReadTextFile(const std::filesystem::path& Path)
{
	std::ifstream File(Path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
}
}

int main()
{
	const std::filesystem::path Root = std::filesystem::current_path();
	const std::filesystem::path ModelDir =
		Root / "Plugins" / "SherpaONNX" / "Content" / "Models" /
		"sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20";

	const std::string Encoder = (ModelDir / "encoder-epoch-13-avg-2-chunk-8-left-64.int8.onnx").generic_string();
	const std::string Decoder = (ModelDir / "decoder-epoch-13-avg-2-chunk-8-left-64.onnx").generic_string();
	const std::string Joiner = (ModelDir / "joiner-epoch-13-avg-2-chunk-8-left-64.int8.onnx").generic_string();
	const std::string Tokens = (ModelDir / "tokens.txt").generic_string();
	const std::string Keywords = ReadTextFile(Root / "Plugins" / "SherpaONNX" / "Content" / "Models" / "keywords.txt");

	SherpaOnnxKeywordSpotterConfig Config{};
	Config.feat_config.sample_rate = 16000;
	Config.feat_config.feature_dim = 80;
	Config.model_config.transducer.encoder = Encoder.c_str();
	Config.model_config.transducer.decoder = Decoder.c_str();
	Config.model_config.transducer.joiner = Joiner.c_str();
	Config.model_config.tokens = Tokens.c_str();
	Config.model_config.num_threads = 1;
	Config.model_config.provider = "cpu";
	Config.model_config.model_type = "";
	Config.max_active_paths = 4;
	Config.num_trailing_blanks = 1;
	Config.keywords_score = 1.0f;
	Config.keywords_threshold = 0.35f;
	Config.keywords_buf = Keywords.c_str();
	Config.keywords_buf_size = static_cast<int32_t>(Keywords.size());

	std::cout << "Calling SherpaOnnxCreateKeywordSpotter..." << std::endl;
	const auto Start = std::chrono::steady_clock::now();
	const SherpaOnnxKeywordSpotter* Spotter = SherpaOnnxCreateKeywordSpotter(&Config);
	const auto ElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - Start).count();

	std::cout << "SherpaOnnxCreateKeywordSpotter returned " << Spotter
		<< " after " << ElapsedMs << " ms" << std::endl;

	if (Spotter)
	{
		SherpaOnnxDestroyKeywordSpotter(Spotter);
	}

	return Spotter ? 0 : 2;
}
