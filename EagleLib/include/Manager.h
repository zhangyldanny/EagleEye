#pragma once
#define CVAPI_EXPORTS

#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <map>
#include <string>
#include "../RuntimeObjectSystem/ObjectInterface.h"
#include "../RuntimeObjectSystem/IObjectFactorySystem.h"
#include <opencv2/core/cvdef.h>
#include <list>
#include <boost/function.hpp>
//#include "nodes/Node.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include "NodeNotifiable.h"

#define ADD_CONSTRUCTORS(managerObj)  \
	auto moduleInterface = PerModuleInterface::GetInstance();	\
	auto vecConstructors = moduleInterface->GetConstructors();	\
	AUDynArray<IObjectConstructor*> dynConstructors;			\
	dynConstructors.Resize(vecConstructors.size());				\
	for (int i = 0; i < vecConstructors.size(); ++i)			\
		dynConstructors[i] = vecConstructors[i];				\
	(managerObj).addConstructors(dynConstructors);

using namespace boost::multi_index;
namespace EagleLib
{
    class Node;
	class Parameter;
    const size_t LOGSYSTEM_MAX_BUFFER = 4096;

    class NodeTreeLeaf;
    CV_EXPORTS struct LeafName{};

    class INodeTreeLeaf: public NodeNotifiable
    {
    public:
        std::string name;
        INodeTreeLeaf(Node* node_, INodeTreeLeaf* parent_ = nullptr);
        virtual Node* getNode() = 0;
        virtual Node* getChildNode(const std::string& name) = 0;
        virtual Node* getChildNode(int idx) = 0;
        virtual Node* getParentNode() = 0;
        virtual INodeTreeLeaf* getParentLeaf() = 0;
        virtual INodeTreeLeaf* getChildLeaf(const std::string& name) = 0;
        virtual INodeTreeLeaf* getChildLeaf(int idx) = 0;
        virtual void addChild(Node* node_) = 0;
        virtual void swapChildren(const std::string& currentName, const std::string& newName) = 0;
        virtual void swapChildren(int initialIdx, int newIdx) = 0;
    };
    typedef boost::multi_index_container<INodeTreeLeaf*, indexed_by<
                                                    boost::multi_index::random_access<>,                                 // Random access indexing to allow changing of order
                                                    hashed_unique<tag<LeafName>, member<INodeTreeLeaf, std::string, &INodeTreeLeaf::name > >
                                                > > NodeTreeLeafContainer;


    class NodeTree
    {
        NodeTreeLeafContainer children;
    public:
        void addChild(Node* node, const std::string& path);
        Node* getNode(const std::string& path);
    };

    class NodeTreeLeaf: public INodeTreeLeaf
    {
    public:
        NodeTreeLeafContainer children;
        NodeTreeLeaf* parent;
        std::string name;
        NodeTreeLeaf(Node* node_, NodeTreeLeaf* parent_ = nullptr);
        virtual Node* getNode();
        virtual Node* getChildNode(const std::string& name);
        virtual Node* getChildNode(int idx);
        virtual Node* getParentNode();
        virtual INodeTreeLeaf* getParentLeaf();
        virtual INodeTreeLeaf* getChildLeaf(const std::string& name);
        virtual INodeTreeLeaf* getChildLeaf(int idx);
        virtual void addChild(Node* node_);
        virtual void swapChildren(const std::string& currentName, const std::string& newName);
        virtual void swapChildren(int initialIdx, int newIdx);
    };


    class CompileLogger: public ICompilerLogger
    {
        char m_buff[LOGSYSTEM_MAX_BUFFER];
        void log(int level, const char* format,  va_list args);

    public:
        boost::function<void(const std::string&, int)> callback;
        virtual void LogError(const char * format, ...);
        virtual void LogWarning(const char * format, ...);
        virtual void LogInfo(const char * format, ...);

    };

    typedef boost::property_tree::basic_ptree<std::string, Node*> t_nodeTree;
    class CV_EXPORTS NodeManager : public IObjectFactoryListener
    {

	public:
		static NodeManager& getInstance();

        Node *addNode(const std::string& nodeName);
        bool removeNode(const std::string& nodeName);
        bool removeNode(ObjectId oid);

		void addConstructors(IAUDynArray<IObjectConstructor*> & constructors);
		void setupModule(IPerModuleInterface* pPerModuleInterface);
        void saveTree(const std::string& fileName);
        bool Init();

        bool MainLoop();

        virtual void OnConstructorsAdded();

        virtual bool CheckRecompile();
        virtual bool CheckRecompile(bool swapAllowed);

        void onNodeRecompile(Node* node);
        Node* getNode(const ObjectId& id);
        Node* getNode(const std::string& treeName);
		void updateTreeName(Node* node, const std::string& prevTreeName);
		void addParameters(Node* node);
		boost::shared_ptr< Parameter > getParameter(const std::string& name);
        void setCompileCallback(boost::function<void(const std::string&, int)> & f);
		// Returns a list of nodes ordered in closeness relative to sourceNode
		// If a tree is something like the following:
		//				A
		//			   /  \
		//			  B    C
		//          / | \    \
		//         D  E  F    G
		// Then calling getNodes("A.B.D", 1) will return a vector with [B,E,F]
		// Calling getNodes("A.B.D", 2) will return a vector with [B,E,F,A,C,G]
		// Calling getNodes("A.B", 0) will return a vector with [D,E,F] in insertion order

		void getSiblingNodes(const std::string& sourceNode, std::vector<Node*>& output);
		void getParentNodes(const std::string& sourceNode, std::vector<Node*>& output);
		void getAccessibleNodes(const std::string& sourceNode, std::vector<Node*>& output);
		Node* getParent(const std::string& sourceNode);
        std::vector<std::string> getConstructableNodes();

	private:
		NodeManager();
		virtual ~NodeManager();
        boost::shared_ptr<IRuntimeObjectSystem>             m_pRuntimeObjectSystem;
        //boost::shared_ptr<ICompilerLogger>                  m_pCompileLogger;
        boost::shared_ptr<CompileLogger>                  m_pCompileLogger;
        std::map<std::string, std::vector<Node*> >          m_nodeMap;


        std::vector<boost::shared_ptr<Node> >               nodeHistory;
        std::list<Node*>                                    deletedNodes;
        std::list<ObjectId>                                 deletedNodeIDs;

		//typedef boost::property_tree::ptree t_nodeTree;
		t_nodeTree m_nodeTree;
		
    }; // class NodeManager
} // namespace EagleLib
