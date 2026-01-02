import gdb

class WatchMember(gdb.Command):
    """Watch a specific member of a struct.
Usage: watch-member <expr> <member>
Example: watch-member myStructPtr real_val
"""

    def __init__(self):
        super(WatchMember, self).__init__("watch-member", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            args = gdb.string_to_argv(arg)
            if len(args) != 2:
                print("Usage: watch-member <expr> <member>")
                return

            expr = args[0]
            member = args[1]

            # Evaluate the expression (pointer to struct)
            struct_ptr = gdb.parse_and_eval(expr)
            if struct_ptr is None:
                print("Error: expression evaluated to NULL")
                return

            # Dereference to get struct
            struct_val = struct_ptr.dereference()

            # Get the member
            member_val = struct_val[member]

            # Compute absolute address of the member
            member_addr = member_val.address

            print("Watching member: %s->%s" % (expr, member))
            print("Member address: %s" % (member_addr))

            # Set hardware watchpoint
            gdb.execute("watch -l *(%s)%s" % (member_val.type.pointer(), member_addr))

        except gdb.error as e:
            print("gdb error: %s" % e)
        except Exception as e:
            print("Python error: %s" % e)

WatchMember()
