try:
    from enum import StrEnum, auto, unique
except ImportError:
    from enum import Enum, EnumMeta, auto, unique
    class _StrEnumMeta(EnumMeta):
        def __contains__(cls, member):
            if isinstance(member, str):
                return member in cls._value2member_map_
            return super().__contains__(member)
    class StrEnum(str, Enum, metaclass=_StrEnumMeta):
        pass


@unique
class MergeTag(StrEnum):
    MOD = "MOD"
    NEW = "NEW"
    NONE = auto()
    ERROR = auto()


@unique
class MergeBranch(StrEnum):
    A = "A"
    B = "B"
    AB = "AB"
    NONE = auto()
    ERROR = auto()


class Node:
    def __init__(self, name: str):
        self.content: dict[str, 'Node'] = {}
        if name.count(":") == 2:
            self.name, tag, branch = name.split(":")
            if tag in MergeTag:
                self.tag = MergeTag(tag)
            else:
                self.tag = MergeTag.ERROR
            if branch in MergeBranch:
                self.branch = MergeBranch(branch)
            else:
                self.branch = MergeBranch.ERROR
        else:
            self.name = name
            self.tag = MergeTag.NONE
            self.branch = MergeBranch.NONE

    def append(self, node: 'Node'):
        self.content[node.name] = node

    def print(self, level: int = 0):
        if self.tag != MergeTag.NONE or self.branch != MergeBranch.NONE:
            print(f"{'-' * (level + 1)} {self.name}:{self.tag}:{self.branch}")
        else:
            print(f"{'-' * (level + 1)} {self.name}")
        for content in self.content:
            self.content[content].print(level + 1)

    def count(self, count_new: bool = False) -> int:
        retval = 1
        for child_name in self.content:
            if ((count_new and self.content[child_name].tag == MergeTag.NEW) or
                    (self.content[child_name].tag != MergeTag.NEW)):
                retval += self.content[child_name].count(count_new)
        return retval

    def compare(self, node: 'Node', compare_new: bool = False) -> int:
        retval = 0
        for child_name in self.content:
            if ((compare_new and self.content[child_name].tag == MergeTag.NEW) or
                    (self.content[child_name].tag != MergeTag.NEW)):
                if child_name in node.content:
                    if compare_new:
                        if (self.content[child_name].name == node.content[child_name].name and
                                self.content[child_name].tag == node.content[child_name].tag and
                                self.content[child_name].branch == node.content[child_name].branch):
                            retval += self.content[child_name].compare(node.content[child_name], compare_new)
                        else:
                            retval += self.content[child_name].count(compare_new)
                    else:
                        retval += self.content[child_name].compare(node.content[child_name], compare_new)
                else:
                    retval += self.content[child_name].count(compare_new)
        return retval


class Tree:
    def __init__(self, filename: str):
        file = open(filename, "r")
        lines = file.readlines()
        file.close()
        last_nodes: list[Node] = []
        first_node = True
        self.errors = []
        for line in lines:
            if line.startswith("-"):
                prefix, name = line.strip("\n").split(" ", 1)
                if len(prefix) != prefix.count("-"):
                    self.errors.append(f"prefix contains foreign characters: {prefix}")
                level = len(prefix) - 1
                node = Node(name)
                if first_node:
                    if level != 0:
                        self.errors.append(f"Wrong initial level: {level}")
                        return
                    first_node = False
                if level == 0:
                    if len(last_nodes) > 0:
                        self.errors.append(f"Root is overwritten: {last_nodes[0].name} -> {node.name}")
                    last_nodes.append(node)
                elif len(last_nodes) <= level:
                    last_nodes.append(node)
                    last_nodes[level - 1].append(node)
                else:
                    last_nodes[level] = node
                    last_nodes[level - 1].append(node)
        if len(last_nodes) > 0:
            self.root = last_nodes[0]
        else:
            self.root = None

    def print(self):
        if self.root is not None:
            self.root.print()
        else:
            print("")

    def count(self, count_new: bool = False):
        if self.root is not None:
            return self.root.count(count_new)
        else:
            return 0

    def compare(self, tree: 'Tree', compare_new: bool = False):
        count1 = self.count(compare_new)
        count2 = tree.count(compare_new)
        if count2 == 0:
            return 0, count1
        if count1 == 0:
            return 0, 0
        return self.root.compare(tree.root, compare_new), count1


def compare_trees(tree1_file_name: str, tree2_file_name: str, compare_new: bool = False):
    tree1 = Tree(tree1_file_name)
    tree2 = Tree(tree2_file_name)
    score1, count1 = tree1.compare(tree2, compare_new)
    score2, count2 = tree2.compare(tree1, compare_new)
    return score1, count1, score2, count2


def get_tree_score(student_output: str, tree_truth: str):
    try:
        diff1, tree_total1, diff2, tree_total2 = compare_trees(student_output, tree_truth, False)
        diff_new1, tree_total_new1, diff_new2, tree_total_new2 = compare_trees(student_output, tree_truth, True)
    except Exception as e:
        print(e)
        return 0.0, 0.0
    diff = diff1 + diff2
    total_tree = tree_total2

    diff_new = diff_new1 + diff_new2 - diff
    total_tree_new = tree_total_new2 - total_tree

    score = max(1.0 - (float(diff) / float(total_tree)), 0.0)
    if diff_new == 0:
        score_new = 1.0
    elif total_tree_new == 0:
        score_new = 0.0
    else:
        score_new = max(1.0 - (float(diff_new) / float(total_tree_new)), 0.0) if total_tree_new > 0 else 1.0

    return score, score_new
