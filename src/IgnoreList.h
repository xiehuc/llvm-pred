#include <set>
#include <string>
#include <fstream>

namespace lle
{
   class IgnoreList: public std::set<std::string>
   {
      private:
      void init_string(const std::string& item, const char dim);
      void init_filename(const std::string& filename);
      public:
      IgnoreList(const std::string& bundle);
   };

   IgnoreList::IgnoreList(const std::string& bundle)
   {
      init_filename(bundle+".txt");
      std::string env = getenv(("IGNORE_" + bundle).c_str()) ?: "";
      init_string(env, ',');
      std::string file = getenv(("IGNORE_" + bundle + "_TXT").c_str()) ?: "";
      init_filename(file);
   }

   void IgnoreList::init_string(const std::string &env, const char dim)
   {
      const size_t npos = std::string::npos;
      size_t pos = 0, end = 0;
      while ((end = env.find(dim, pos)) != npos) {
         this->emplace(env, pos, end - pos);
         pos = end + 1;
      }
      if (env.compare(pos, npos, "") != 0)
         this->emplace(env, pos, end - pos);
   }

   void IgnoreList::init_filename(const std::string &filename)
   {
      std::ifstream ifs(filename);
      if (!ifs.is_open()) return;
      std::string line;
      while (ifs >> line) {
         if (line != "")
            this->insert(line);
      }
      ifs.close();
   }
}
