/* Author: Yohei Kakiuchi */
#include <ros/ros.h>
#include "collada_parser/collada_parser.h"
#include "urdf/model.h"

#if IS_ASSIMP3
// assimp 3 (assimp_devel)
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#else
// assimp 2
#include <assimp/assimp.hpp>
#include <assimp/aiScene.h>
#include <assimp/aiPostProcess.h>
#include <assimp/IOStream.h>
#include <assimp/IOSystem.h>
#endif

#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>

#include <tf/LinearMath/Transform.h>
#include <tf/LinearMath/Quaternion.h>

#include <resource_retriever/retriever.h>

#include "yaml-cpp/yaml.h"

// copy from rviz/src/rviz/mesh_loader.cpp
class ResourceIOStream : public Assimp::IOStream
{
public:
  ResourceIOStream(const resource_retriever::MemoryResource& res)
  : res_(res)
  , pos_(res.data.get())
  {}

  ~ResourceIOStream()
  {}

  size_t Read(void* buffer, size_t size, size_t count)
  {
    size_t to_read = size * count;
    if (pos_ + to_read > res_.data.get() + res_.size)
    {
      to_read = res_.size - (pos_ - res_.data.get());
    }

    memcpy(buffer, pos_, to_read);
    pos_ += to_read;

    return to_read;
  }

  size_t Write( const void* buffer, size_t size, size_t count) { ROS_BREAK(); return 0; }

  aiReturn Seek( size_t offset, aiOrigin origin)
  {
    uint8_t* new_pos = 0;
    switch (origin)
    {
    case aiOrigin_SET:
      new_pos = res_.data.get() + offset;
      break;
    case aiOrigin_CUR:
      new_pos = pos_ + offset; // TODO is this right?  can offset really not be negative
      break;
    case aiOrigin_END:
      new_pos = res_.data.get() + res_.size - offset; // TODO is this right?
      break;
    default:
      ROS_BREAK();
    }

    if (new_pos < res_.data.get() || new_pos > res_.data.get() + res_.size)
    {
      return aiReturn_FAILURE;
    }

    pos_ = new_pos;
    return aiReturn_SUCCESS;
  }

  size_t Tell() const
  {
    return pos_ - res_.data.get();
  }

  size_t FileSize() const
  {
    return res_.size;
  }

  void Flush() {}

private:
  resource_retriever::MemoryResource res_;
  uint8_t* pos_;
};

class ResourceIOSystem : public Assimp::IOSystem
{
public:
  ResourceIOSystem()
  {
  }

  ~ResourceIOSystem()
  {
  }

  // Check whether a specific file exists
  bool Exists(const char* file) const
  {
    // Ugly -- two retrievals where there should be one (Exists + Open)
    // resource_retriever needs a way of checking for existence
    // TODO: cache this
    resource_retriever::MemoryResource res;
    try
    {
      res = retriever_.get(file);
    }
    catch (resource_retriever::Exception& e)
    {
      return false;
    }

    return true;
  }

  // Get the path delimiter character we'd like to see
  char getOsSeparator() const
  {
    return '/';
  }

  // ... and finally a method to open a custom stream
  Assimp::IOStream* Open(const char* file, const char* mode = "rb")
  {
    ROS_ASSERT(mode == std::string("r") || mode == std::string("rb"));

    // Ugly -- two retrievals where there should be one (Exists + Open)
    // resource_retriever needs a way of checking for existence
    resource_retriever::MemoryResource res;
    try
    {
      res = retriever_.get(file);
    }
    catch (resource_retriever::Exception& e)
    {
      return 0;
    }

    return new ResourceIOStream(res);
  }

  void Close(Assimp::IOStream* stream);

private:
  mutable resource_retriever::Retriever retriever_;
};

void ResourceIOSystem::Close(Assimp::IOStream* stream)
{
  delete stream;
}

////
using namespace urdf;
using namespace std;

#define FLOAT_PRECISION_FINE   "%.16e"
#define FLOAT_PRECISION_COARSE "%.3f"
//
class ModelEuslisp {

public:
  ModelEuslisp () { } ;
  ModelEuslisp (boost::shared_ptr<ModelInterface> r);
  ~ModelEuslisp ();

  void setRobotName (string &name) { arobot_name = name; };
  void setUseCollision(bool &b) { use_collision = b; };
  void setUseSimpleGeometry(bool &b) { use_simple_geometry = b; };
  void setAddJointSuffix(bool &b) { add_joint_suffix = b; };
  void setAddLinkSuffix(bool &b) { add_link_suffix = b; };
  void writeToFile (string &filename);
  void addLinkCoords();
  void addChildJointNames(boost::shared_ptr<const Link> link);

  Pose getLinkPose(boost::shared_ptr<const Link> link) {
    if (!link->parent_joint) {
      Pose ret;
      return ret;
    }
    Pose p_pose = getLinkPose(link->getParent());
    Pose l_pose = link->parent_joint->parent_to_joint_origin_transform;
    Pose ret;
    ret.rotation = p_pose.rotation * l_pose.rotation;
    ret.position = (p_pose.rotation * l_pose.position) +  p_pose.position;

    return ret;
  }
  void printJoint (boost::shared_ptr<const Joint> joint);
  void printLink (boost::shared_ptr<const Link> Link, Pose &pose);
  void printLinks ();
  void printJoints ();
  void printGeometries();
  void printMesh(const aiScene* scene, const aiNode* node);

  void readYaml(string &config_file);
private:
  typedef map <string, boost::shared_ptr<const Visual> > MapVisual;
  typedef map <string, boost::shared_ptr<const Collision> > MapCollision;
  typedef pair<vector<string>, vector<string> > link_joint;
  typedef pair<string, link_joint > link_joint_pair;

  boost::shared_ptr<ModelInterface> robot;
  string arobot_name;
  map <boost::shared_ptr<const Link>, Pose > m_link_coords;
  map <boost::shared_ptr<const Link>, MapVisual > m_link_visual;
  map <boost::shared_ptr<const Link>, MapCollision > m_link_collision;
  map <string, boost::shared_ptr<const Material> > m_materials;
  vector<pair<string, string> > g_all_link_names;
  FILE *fp;
  YAML::Node doc;

  //
  bool add_joint_suffix;
  bool add_link_suffix;
  bool use_simple_geometry;
  bool use_collision;
};

ModelEuslisp::ModelEuslisp (boost::shared_ptr<ModelInterface> r) {
  robot = r;
  add_joint_suffix = false;
  add_link_suffix = false;
  use_simple_geometry = false;
  use_collision = false;
}

ModelEuslisp::~ModelEuslisp () {

}

void ModelEuslisp::addLinkCoords() {
  for (std::map<std::string, boost::shared_ptr<Link> >::iterator link = robot->links_.begin();
       link != robot->links_.end(); link++) {
    Pose p = getLinkPose(link->second);
    m_link_coords.insert
      (map<boost::shared_ptr<const Link>, Pose >::value_type (link->second, p));
#if DEBUG
    std::cerr << "name: " << link->first;
    std::cerr << ", #f(" << p.position.x << " ";
    std::cerr << p.position.y << " ";
    std::cerr << p.position.z << ") #f(";
    std::cerr << p.rotation.w << " ";
    std::cerr << p.rotation.x << " ";
    std::cerr << p.rotation.y << " ";
    std::cerr << p.rotation.z << ")" << std::endl;
#endif
    if (use_collision) {
      //link->second->collision;
      //link->second->collision_array;
    } else {
      if(!!link->second->visual) {
        m_materials.insert
          (map <string, boost::shared_ptr<const Material> >::value_type
           (link->second->visual->material_name, link->second->visual->material));
        MapVisual mv;
        string gname(link->second->name);
        gname += "_geom0";
        mv.insert(MapVisual::value_type (gname, link->second->visual));

        m_link_visual.insert
          (map <boost::shared_ptr<const Link>, MapVisual >::value_type
           (link->second, mv));
      } else {
        int counter = 0;
        MapVisual mv;
        for (std::vector<boost::shared_ptr <Visual> >::iterator it = link->second->visual_array.begin();
             it != link->second->visual_array.end(); it++) {
          m_materials.insert
            (map <string, boost::shared_ptr<const Material> >::value_type ((*it)->material_name, (*it)->material));
          MapVisual mv;
          stringstream ss;
          ss << link->second->name << "_geom" << counter;
          mv.insert(MapVisual::value_type (ss.str(), (*it)));
          counter++;
        }
        m_link_visual.insert
          (map <boost::shared_ptr<const Link>, MapVisual >::value_type (link->second, mv));
      }
    }
  }
}

void ModelEuslisp::printLinks () {

}

void ModelEuslisp::printLink (boost::shared_ptr<const Link> link, Pose &pose) {

}

void ModelEuslisp::printJoints () {
  for (std::map<std::string, boost::shared_ptr<Joint> >::iterator joint = robot->joints_.begin();
       joint != robot->joints_.end(); joint++) {
    printJoint(joint->second);
  }
}

void ModelEuslisp::printJoint (boost::shared_ptr<const Joint> joint) {
  bool linear = (joint->type==Joint::PRISMATIC);
  if (joint->type != Joint::REVOLUTE && joint->type !=Joint::CONTINUOUS
      && joint->type !=Joint::PRISMATIC && joint->type != Joint::FIXED) {
    // error
  }

  if (add_joint_suffix) {
    fprintf(fp, "     (setq %s_jt\n", joint->name.c_str());
  } else {
    fprintf(fp, "     (setq %s\n", joint->name.c_str());
  }
  fprintf(fp, "           (instance %s :init\n", linear?"linear-joint":"rotational-joint");
  fprintf(fp, "                     :name \"%s\"\n", joint->name.c_str());
  if (add_link_suffix) {
    fprintf(fp, "                     :parent-link %s_lk :child-link %s_lk\n",
            joint->parent_link_name.c_str(), joint->child_link_name.c_str());
  } else {
    fprintf(fp, "                     :parent-link %s :child-link %s\n",
            joint->parent_link_name.c_str(), joint->child_link_name.c_str());
  }
  { // axis
    fprintf(fp, "                     :axis ");
    fprintf(fp, "(float-vector "FLOAT_PRECISION_FINE" "FLOAT_PRECISION_FINE" "FLOAT_PRECISION_FINE")\n",
            joint->axis.x, joint->axis.y, joint->axis.z);
  }
  {
    float min = joint->limits->lower;
    float max = joint->limits->upper;
    fprintf(fp, "                     ");
    fprintf(fp, ":min "); if (min == FLT_MAX) fprintf(fp, "*-inf*"); else fprintf(fp, "%f", min);
    fprintf(fp, " :max "); if (max ==-FLT_MAX) fprintf(fp,  "*inf*"); else fprintf(fp, "%f", max);
    fprintf(fp, "\n");
    fprintf(fp, "                     :max-joint-velocity %f\n", joint->limits->velocity);
    fprintf(fp, "                     :max-joint-torque %f\n", joint->limits->effort);
  }
  fprintf(fp, "                     ))\n");
}

void ModelEuslisp::printGeometries () {
  if (use_collision) {

  } else {
    for(map <boost::shared_ptr<const Link>, MapVisual >::iterator it = m_link_visual.begin();
        it != m_link_visual.end(); it++) {
      //it->first
      for( MapVisual::iterator vmap = it->second.begin();
           vmap != it->second.end(); vmap++) {
        string gname = vmap->first;
        fprintf(fp, "(defclass %s_%s\n", arobot_name.c_str(), gname.c_str());
        fprintf(fp, "  :super collada-body\n");
        fprintf(fp, "  :slots ())\n");
        fprintf(fp, "(defmethod %s_%s\n", arobot_name.c_str(), gname.c_str());
        if (false)  {
          fprintf(fp, "  (:init (&key (name))\n");
          fprintf(fp, "         (replace-object self (make-cube 10 10 10))\n");
          fprintf(fp, "         (if name (send self :name name))\n");
          fprintf(fp, "         self)\n");
          fprintf(fp, "   )\n");
          return;
        }

        // Pose p = vmap->second->origin;
        boost::shared_ptr<Geometry> g = vmap->second->geometry;
        if (g->type == Geometry::MESH) {
          gname = ((Mesh *)(g.get()))->filename;
        }
        if (g->type == Geometry::SPHERE) {
          //
        } else if (g->type == Geometry::BOX) {
          //
        } else if (g->type == Geometry::CYLINDER) {
          //
        } else {
          fprintf(fp, "  (:init (&key (name \"%s\"))\n", gname.c_str());
          fprintf(fp, "         (replace-object self (send self :qhull-faceset))\n");
          fprintf(fp, "         (if name (send self :name name))\n");
          fprintf(fp, "         (send self :def-gl-vertices)\n");
          fprintf(fp, "         self)\n");

          Assimp::Importer importer;
          importer.SetIOHandler(new ResourceIOSystem());
          const aiScene* scene = importer.ReadFile(gname,
                                                   aiProcess_SortByPType|aiProcess_GenNormals|aiProcess_Triangulate|
                                                   aiProcess_GenUVCoords|aiProcess_FlipUVs);
          if (scene && scene->HasMeshes()) {
            fprintf(fp, "  (:def-gl-vertices ()\n");
            fprintf(fp, "    (setq glvertices\n");
            fprintf(fp, "       (instance gl::glvertices :init\n");
            fprintf(fp, "          (list\n"); // mesh-list
            printMesh(scene, scene->mRootNode);
          } else {
            // error
          }
        }
      }
    }
  }
}

void ModelEuslisp::printMesh(const aiScene* scene, const aiNode* node) {
  aiMatrix4x4 transform = node->mTransformation;
  aiNode *pnode = node->mParent;
  while (pnode)  {
    if (pnode->mParent != NULL) {
      transform = pnode->mTransformation * transform;
    }
    pnode = pnode->mParent;
  }

  aiMatrix3x3 rotation(transform);
  aiMatrix3x3 inverse_transpose_rotation(rotation);
  inverse_transpose_rotation.Inverse();
  inverse_transpose_rotation.Transpose();

  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    aiMesh* input_mesh = scene->mMeshes[node->mMeshes[i]];
    // normals
    if (input_mesh->HasNormals())  {

    }
    // texture coordinates (only support 1 for now)
    if (input_mesh->HasTextureCoords(0))  {

    }
    // todo vertex colors
    //input_mesh->mNumVertices;
    // allocate the vertex buffer

    // Add the vertices
    for (uint32_t j = 0; j < input_mesh->mNumVertices; j++)  {
      aiVector3D p = input_mesh->mVertices[j];
      p *= transform;
      //p *= scale;
      //*vertices++ = p.x;
      //*vertices++ = p.y;
      //*vertices++ = p.z;

      if (input_mesh->HasNormals()) {
        aiVector3D n = inverse_transpose_rotation * input_mesh->mNormals[j];
        n.Normalize();
        //*vertices++ = n.x;
        //*vertices++ = n.y;
        //*vertices++ = n.z;
      }

      if (input_mesh->HasTextureCoords(0)) {
        //*vertices++ = input_mesh->mTextureCoords[0][j].x;
        //*vertices++ = input_mesh->mTextureCoords[0][j].y;
      }
    }

    // calculate index count
    for (uint32_t j = 0; j < input_mesh->mNumFaces; j++) {
      aiFace& face = input_mesh->mFaces[j];
      for (uint32_t k = 0; k < face.mNumIndices; ++k) {
        //*indices++ = face.mIndices[k];
      }
    }

    for (uint32_t i=0; i < node->mNumChildren; ++i) {
      printMesh(scene, node->mChildren[i]);
    }
  }
}

void ModelEuslisp::writeToFile (string &filename) {
  if (!robot) {
    cerr << ";; not robot" << std::endl;
    return;
  }

  fp = fopen(filename.c_str(),"w");

  if (fp == NULL) {
    return;
  }

  addLinkCoords();

  // start print
  printLinks();

  printJoints();

  printGeometries();
  //printLinkAccessor();
  //printJointAccessor();
}

bool limb_order_asc(const pair<string, size_t>& left, const pair<string, size_t>& right) { return left.second < right.second; }
void ModelEuslisp::readYaml (string &config_file) {
  // read yaml
  string limb_candidates[] = {"torso", "larm", "rarm", "lleg", "rleg", "head"}; // candidates of limb names

  vector<pair<string, size_t> > limb_order;
  ifstream fin(config_file.c_str());
  if (fin.fail()) {
    fprintf(stderr, "%c[31m;; Could not open %s%c[m\n", 0x1b, config_file.c_str(), 0x1b);
  } else {
    YAML::Parser parser(fin);
    parser.GetNextDocument(doc);

    /* re-order limb name by lines of yaml */
    BOOST_FOREACH(string& limb, limb_candidates) {
      if ( doc.FindValue(limb) ) {
        std::cerr << limb << "@" << doc[limb].GetMark().line << std::endl;
        limb_order.push_back(pair<string, size_t>(limb, doc[limb].GetMark().line));
      }
    }
    std::sort(limb_order.begin(), limb_order.end(), limb_order_asc);
  }

  // generate limbs including limb_name, link_names, and joint_names
  vector<link_joint_pair> limbs;
  for (size_t i = 0; i < limb_order.size(); i++) {
    string limb_name = limb_order[i].first;
    vector<string> tmp_link_names, tmp_joint_names;
    try {
      const YAML::Node& limb_doc = doc[limb_name];
      for(unsigned int i = 0; i < limb_doc.size(); i++) {
        const YAML::Node& n = limb_doc[i];
        for(YAML::Iterator it=n.begin();it!=n.end();it++) {
          string key, value; it.first() >> key; it.second() >> value;
          tmp_joint_names.push_back(key);
          //tmp_link_names.push_back(findChildLinkFromJointName(key.c_str())->getName());
          g_all_link_names.push_back(pair<string, string>(key, value));
        }
      }
      limbs.push_back(link_joint_pair(limb_name, link_joint(tmp_link_names, tmp_joint_names)));
    } catch(YAML::RepresentationException& e) {
    }
  }

}
namespace po = boost::program_options;
int main(int argc, char** argv)
{
  bool use_simple_geometry;
  bool use_collision;
  string arobot_name;

  string input_file;
  string config_file;
  string output_file;

  po::options_description desc("Options for collada_to_urdf");
  desc.add_options()
    ("help", "produce help message")
    ("simple_geometry,V", "use bounding box for geometry")
    ("use_collision,U", "use collision geometry (default collision is the same as visual)")
    ("robot_name,N", po::value< vector<string> >(), "output robot name")
    ("input_file,I", po::value< vector<string> >(), "input file")
    ("config_file,C", po::value< vector<string> >(), "configuration yaml file")
    ("output_file,O", po::value< vector<string> >(), "output file")
    ;

  po::positional_options_description p;
  p.add("input_file", 1);
  p.add("config_file", 1);
  p.add("output_file", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).
              options(desc).positional(p).run(), vm);
    po::notify(vm);
  }
  catch (po::error e) {
    cerr << ";; option parse error / " << e.what() << endl;
    return 1;
  }

  if (vm.count("help")) {
    cout << desc << "\n";
    return 1;
  }
  if (vm.count("simple_geometry")) {
    use_simple_geometry = true;
    cerr << ";; Using simple_geometry" << endl;
  }
  if (vm.count("use_collision")) {
    use_collision = true;
    cerr << ";; Using simple_geometry" << endl;
  }
  if (vm.count("output_file")) {
    vector<string> aa = vm["output_file"].as< vector<string> >();
    cerr << ";; output file is: "
         <<  aa[0] << endl;
    output_file = aa[0];
  }
  if (vm.count("robot_name")) {
    vector<string> aa = vm["robot_name"].as< vector<string> >();
    cerr << ";; robot_name is: "
         <<  aa[0] << endl;
    arobot_name = aa[0];
  }
  if (vm.count("input_file")) {
    vector<string> aa = vm["input_file"].as< vector<string> >();
    cerr << ";; Input file is: "
         <<  aa[0] << endl;
    input_file = aa[0];
  }
  if (vm.count("config_file")) {
    vector<string> aa = vm["config_file"].as< vector<string> >();
    cerr << ";; Config file is: "
         <<  aa[0] << endl;
    config_file = aa[0];
  }

  std::string xml_string;
  std::fstream xml_file(input_file.c_str(), std::fstream::in);
  while ( xml_file.good() )
  {
    std::string line;
    std::getline( xml_file, line);
    xml_string += (line + "\n");
  }
  xml_file.close();

  boost::shared_ptr<ModelInterface> robot;
  if( xml_string.find("<COLLADA") != std::string::npos )
  {
    ROS_DEBUG("Parsing robot collada xml string");
    robot = parseCollada(xml_string);
  }
  else
  {
    ROS_DEBUG("Parsing robot urdf xml string");
    robot = parseURDF(xml_string);
  }

  if (!robot){
    std::cerr << "ERROR: Model Parsing the xml failed" << std::endl;
    return -1;
  }

  if (arobot_name == "") {
    arobot_name = robot->getName();
  }
  if (output_file == "") {
    output_file =  arobot_name + ".urdf";
  }

  ModelEuslisp eusmodel(robot);
  eusmodel.setRobotName(arobot_name);
  eusmodel.writeToFile (output_file);
}
