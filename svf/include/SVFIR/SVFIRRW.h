#ifndef INCLUDE_SVFIRRW_H_
#define INCLUDE_SVFIRRW_H_

#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include "Graphs/GenericGraph.h"
//#include "Graphs/ICFG.h"
//#include "Graphs/IRGraph.h"
#include <type_traits>

#define ABORT_IFNOT(condition, reason)                                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            SVFUtil::errs()                                                    \
                << __FILE__ << ':' << __LINE__ << ": " << reason << '\n';      \
            abort();                                                           \
        }                                                                      \
    } while (0)

/// @brief Type trait to check if a type is iterable.
///@{
template <typename T>
decltype(std::begin(std::declval<T&>()) !=
             std::end(std::declval<T&>()), // begin/end and operator!=
         void(),                           // Handle evil operator,
         ++std::declval<decltype(begin(std::declval<T&>()))&>(), // operator++
         *begin(std::declval<T&>()),                             // operator*
         std::true_type{})
is_iterable_impl(int);
template <typename T> std::false_type is_iterable_impl(...);

template <typename T>
using is_iterable = decltype(is_iterable_impl<T>(0));
template <typename T> constexpr bool is_iterable_v = is_iterable<T>::value;
///@}

/// @brief Type trait to check if a type is a generic graph writer.
/// @{
template <typename T>
using is_generic_graph_writer =
    std::is_same<typename T::GenericGraphWriterTraitType, void>;
template <typename T>
constexpr bool is_generic_graph_writer_v = is_generic_graph_writer<T>::value;
/// @}


namespace SVF
{

cJSON* jsonCreateObject();
cJSON* jsonCreateArray();
cJSON* jsonCreateMap();
cJSON* jsonCreateString(const char* str);
cJSON* jsonCreateIndex(size_t index);
cJSON* jsonCreateNumber(double num);
bool jsonAddPairToMap(cJSON* obj, cJSON* key, cJSON* value);
bool jsonAddItemToObject(cJSON* obj, const char* name, cJSON* item);
bool jsonAddItemToArray(cJSON* array, cJSON* item);
/// @brief Helper function to write a number to a JSON object.
bool jsonAddNumberToObject(cJSON* obj, const char* name, double number);
bool jsonAddStringToObject(cJSON* obj, const char* name, const char* str);
bool jsonAddStringToObject(cJSON* obj, const char* name,
                           const std::string& str);

#define JSON_WRITE_NUMBER_FIELD(root, objptr, field)                           \
    jsonAddNumberToObject(root, #field, (objptr)->field)
#define JSON_WRITE_STRING_FIELD(root, objptr, field)                           \
    jsonAddStringToObject(root, #field, (objptr)->field)
#define JSON_WRITE_FIELD(root, objptr, field)                                  \
    jsonAddJsonableToObject(root, #field, (objptr)->field)


class SVFIR;
class SVFIRWriter;
class CHNode;
class CHEdge;
class CHGraph;

template <typename T>
class PtrPool
{
private:
    Map<const T*, size_t> ptrToId;
    std::vector<const T*> ptrPool;

public:
    inline size_t getID(const T* ptr)
    {
        if (ptr == nullptr)
            return 0;
        auto it_inserted = ptrToId.emplace(ptr, 1 + ptrPool.size());
        if (it_inserted.second)
            ptrPool.push_back(ptr);
        return it_inserted.first->second;
    }

    inline void saveID(const T* ptr)
    {
        getID(ptr);
    }

    inline const T* getPtr(size_t id) const
    {
        assert(id >= 0 && id <= ptrPool.size() && "Invalid ID.");
        return id ? ptrPool[id - 1] : nullptr;
    }

    inline const std::vector<const T*>& getPool() const
    {
        return ptrPool;
    }
};

template <typename NodeTy, typename EdgeTy>
class GenericGraphWriter
{
    friend class SVFIRWriter;
private:
    using GenericGraphWriterTraitType = void;
    using NodeType = NodeTy;
    using EdgeType = EdgeTy;
    using GraphType = GenericGraph<NodeType, EdgeType>;

    const GraphType* graph;
    OrderedMap<const NodeType*, NodeID> nodeToID;
    PtrPool<EdgeType> edgePool;

public:
    GenericGraphWriter(const GraphType* g) : graph(g)
    {
        for (const auto& entry : graph->IDToNodeMap)
        {
            const NodeID id = entry.first;
            const NodeType* node = entry.second;

            nodeToID.emplace(node, id);
            for (const EdgeType* edge : node->getOutEdges())
            {
                edgePool.saveID(edge);
            }
        }
    }

    inline size_t getEdgeID(const EdgeType* edge)
    {
        return edgePool.getID(edge);
    }

    inline NodeID getNodeID(const NodeType* node) const
    {
        auto it = nodeToID.find(node);
        assert(it != nodeToID.end() && "Node not found in the graph.");
        return it->second;
    }
};

using GenericICFGWriter = GenericGraphWriter<ICFGNode, ICFGEdge>;

class ICFGWriter : public GenericICFGWriter
{
private:
    PtrPool<SVFLoop> svfLoopPool;

public:
    ICFGWriter(const ICFG* icfg);
};

using IRGraphWriter = GenericGraphWriter<SVFVar, SVFStmt>;
using CHGraphWriter = GenericGraphWriter<CHNode, CHEdge>;

struct CommonCHGraphWriter
{
    CommonCHGraphWriter(const CommonCHGraph *chg);
    ~CommonCHGraphWriter();

    const CHGraphWriter* chGraphWriter;
};

class SVFIRWriter
{
    const SVFIR* svfIR;

    PtrPool<SVFType> svfTypePool;
    PtrPool<SVFValue> svfValuePool;

    IRGraphWriter irGraphWriter;
    ICFGWriter icfgWriter;
    CommonCHGraphWriter commonCHGraphWriter;

    OrderedMap<size_t, std::string> numToStrMap;

public:
    SVFIRWriter(const SVFIR* svfir);

    cJSON *toJson(const SVFModule* module);

    const char* generateJsonString();

private:
    /// @brief Main logic to dump a SVFIR to a JSON object.
    cJSON* generateJson();
    const char* numToStr(size_t n);

    cJSON* toJson(const SVFType* type);
    cJSON* toJson(const SVFValue* value);
    cJSON* toJson(const IRGraph* graph); // IRGraph Graph
    cJSON* toJson(const SVFVar* var);    // IRGraph Node
    cJSON* toJson(const SVFStmt* stmt);  // IRGraph Edge
    cJSON* toJson(const ICFG* icfg);     // ICFG Graph
    cJSON* toJson(const ICFGNode* node); // ICFG Node
    cJSON* toJson(const ICFGEdge* edge); // ICFG Edge
    cJSON* toJson(const CHGraph* graph); // CHGraph Graph
    cJSON* toJson(const CHNode* node); // CHGraph Node
    cJSON* toJson(const CHEdge* edge); // CHGraph Edge
    cJSON* toJson(const SVFLoop* loop); // TODO

    static cJSON* toJson(const LocationSet& ls);
    static cJSON* toJson(unsigned number);
    static cJSON* toJson(int number);
    static cJSON* toJson(long long number);

    /// \brief Parameter types of these functions are all pointers.
    /// When they are used as arguments of toJson(), they will be
    /// dumped as an index. `contentToJson()` will dump the actual content.
    ///@{
    cJSON* virtToJson(const SVFVar* var);
    cJSON* virtToJson(const SVFStmt* stmt);
    cJSON* virtToJson(const ICFGNode* node);
    cJSON* virtToJson(const ICFGEdge* edge);
    cJSON* virtToJson(const CHNode* node);
    cJSON* virtToJson(const CHEdge* edge);

    cJSON* contentToJson(const ICFGNode* node);
    cJSON* contentToJson(const GlobalICFGNode* node);
    cJSON* contentToJson(const IntraICFGNode* node);
    cJSON* contentToJson(const InterICFGNode* node);
    cJSON* contentToJson(const FunEntryICFGNode* node);
    cJSON* contentToJson(const FunExitICFGNode* node);
    cJSON* contentToJson(const CallICFGNode* node);
    cJSON* contentToJson(const RetICFGNode* node);
    ///@}

    template <typename NodeTy, typename EdgeTy>
    cJSON* genericNodeToJson(const GenericNode<NodeTy, EdgeTy>* node)
    {
        cJSON* root = jsonCreateObject();
        JSON_WRITE_FIELD(root, node, id);
        JSON_WRITE_FIELD(root, node, nodeKind);
        JSON_WRITE_FIELD(root, node, InEdges);
        JSON_WRITE_FIELD(root, node, OutEdges);
        return root;
    }

    template <typename NodeTy, typename EdgeTy>
    cJSON* genericGraphToJson(const GenericGraph<NodeTy, EdgeTy>* graph,
                              const std::vector<const EdgeTy*>& edgePool)
    {
        cJSON* root = jsonCreateObject();

        JSON_WRITE_FIELD(root, graph, edgeNum);
        JSON_WRITE_FIELD(root, graph, nodeNum);

        cJSON* map = jsonCreateMap();
        for (const auto& pair : graph->IDToNodeMap)
        {
            NodeID id = pair.first;
            NodeTy* node = pair.second;

            cJSON* jsonID = jsonCreateIndex(id);
            cJSON* jsonNode = virtToJson(node);
            jsonAddPairToMap(map, jsonID, jsonNode);
        }

        cJSON* edgesJson = jsonCreateArray();
        for (const EdgeTy* edge : edgePool)
        {
            cJSON* edgeJson = virtToJson(edge);
            jsonAddItemToArray(edgesJson, edgeJson);
        }
        jsonAddItemToObject(root, "edges", edgesJson);

        return root;
    }

    template <unsigned ElementSize>
    static cJSON* toJson(const SparseBitVector<ElementSize>& bv)
    {
        return cJSON_CreateString("TODO: JSON BitVector");
    }

    template <typename T, typename U>
    cJSON* toJson(const std::pair<T, U>& pair)
    {
        cJSON* obj = jsonCreateObject();
        const auto* p = &pair;
        JSON_WRITE_FIELD(obj, p, first);
        JSON_WRITE_FIELD(obj, p, second);
        return obj;
    }

    template <typename T, typename = std::enable_if_t<is_iterable_v<T>>>
    cJSON* toJson(const T& container)
    {
        cJSON* array = jsonCreateArray();
        for (const auto& item : container)
        {
            cJSON* itemObj = toJson(item);
            jsonAddItemToArray(array, itemObj);
        }
        return array;
    }

    template <typename T>
    bool jsonAddJsonableToObject(cJSON* obj, const char* name, const T& item)
    {
        cJSON* itemObj = toJson(item);
        return jsonAddItemToObject(obj, name, itemObj);
    }
};

} // namespace SVF

#endif // !INCLUDE_SVFIRRW_H_