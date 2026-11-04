import unittest
from prepare_sources import exported_names, externalize


class GccExports(unittest.TestCase):
    def test_asm_names_cover_functions_arrays_and_pointers(self):
        self.assertEqual(exported_names('int f(int x) asm("F");\nextern Foo *ptr asm("P");\nextern int array[4] asm("A");'), {"f", "ptr", "array"})

    def test_only_exported_file_scope_storage_changes(self):
        original = 'static int shared;\nstatic int other;\nstatic int f() { static int shared; return shared; }\n'
        result, edits = externalize(original, {"shared"})
        self.assertEqual(result, original.replace('static int shared;', '       int shared;', 1))
        self.assertEqual([name for _, name in edits], ['shared'])

    def test_prototype_and_definition_share_linkage(self):
        source = 'static int f(int);\nstatic int f(int x) { return x; }\n'
        result, edits = externalize(source, {"f"})
        self.assertEqual(result.count('static'), 0)
        self.assertEqual(len(edits), 2)

    def test_comments_literals_and_members_stay_intact(self):
        source = '// static int f;\nstruct S { static int f; };\nconst char *s = "static int f;";\n'
        self.assertEqual(externalize(source, {"f"}), (source, []))


if __name__ == '__main__':
    unittest.main()
