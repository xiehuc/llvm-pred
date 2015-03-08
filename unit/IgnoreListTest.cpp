#include <functional>
#include <gtest/gtest.h>
#define private public
#include "IgnoreList.h"

using namespace lle;
TEST(IgnoreTest, ConstrucString)
{
   auto init_entry = std::mem_fn(&IgnoreList::init_string);
   std::set<std::string> set, empty;
   init_entry((IgnoreList*)&set, "a,bc,def", ',');

   EXPECT_TRUE(set.count("a"));
   EXPECT_TRUE(set.count("bc"));
   EXPECT_TRUE(set.count("def"));

   init_entry((IgnoreList*)&empty, "", ',');
   EXPECT_EQ(empty.size(), 0);
}

TEST(IgnoreTest, ConstrucFile)
{
   const char* FILE_NAME = "test.log";
   std::ofstream ofs(FILE_NAME);
   ofs<<"a\nbc\ndef\n";
   ofs.close();

   std::set<std::string> set, empty;
   auto init_file = std::mem_fn(&IgnoreList::init_filename);
   init_file((IgnoreList*)&set, FILE_NAME);

   EXPECT_TRUE(set.count("a"));
   EXPECT_TRUE(set.count("bc"));
   EXPECT_TRUE(set.count("def"));

   unlink(FILE_NAME);

   init_file((IgnoreList*)&empty, "not_exist");
   EXPECT_EQ(empty.size(), 0);
}
