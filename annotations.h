#include "core.h"
#include "wave_data_base.h"
#include <future>
#include <map>
#include <string>

struct FstFile;
struct Node;

using ModulePath = std::vector<std::string>;

class AnnotationData {
	bool is_event;
	std::string text;
	WaveDatabase data;
};

class AnnotationSpan {
	simtime_t start;
	simtime_t end;
};

class Annotation {
	ModulePath module_path;
	bool is_event;
	std::string text;
	std::vector<AnnotationSpan> times;
};

class Annotations {
	std::map<std::shared_ptr<Node>, std::vector<Annotation>>
};

class AnnotationsData
{
	using DataT = std::map<ModulePath, std::vector<Annotation>>;

	std::future<DataT> data_future;
	std::optional<DataT> data;

public:
	AnnotationsData(std::shared_ptr<FstFile> fst_file, std::vector<std::shared_ptr<Node>> nodes);
	std::optional<Annotations> query(std::vector<ModulePath> paths);
};
