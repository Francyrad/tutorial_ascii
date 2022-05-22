#!/usr/bin/python3

import json
from sys import argv, exit, stderr

def handle_subsection(data, cur_path, output_file):
    keys = list(data.keys())
    keys.sort()
    # Since there is no end to a markdown heading besides another markdown
    # heading of equal or greater size, we need to first do parameters and
    # aliases, and then do subsections otherwise parameters get nested
    # incorrectly
    for key in keys:
        path_str = "/".join(cur_path) + "/"  + key
        true_name = key.replace("_20", " ")
        
        if "value" in data[key]:
            # this is a parameter
            print("(parameters:" + path_str + ")=", file=output_file)
            print("### __Parameter name:__ " + true_name, file=output_file)
            print("**Default value:** " +  data[key]["default_value"] + " \n", file=output_file)
            print("**Pattern:** " +  data[key]["pattern_description"] + " \n", file=output_file)
            print("**Documentation:** " + data[key]["documentation"] + " \n", file=output_file)
        elif "alias" in data[key]:
            # This is an alias for a parameter
            print("(parameters:" + path_str + ")=", file=output_file)
            aliased_name = data[key]["alias"]
            alias_path_str = "/".join(cur_path) + "/" + aliased_name.replace(" ", "_20") 
            print("### __Parameter name__: " +  true_name, file=output_file)
            print("**Alias:** [" + aliased_name + "](parameters:" + alias_path_str + ")\n", file=output_file)
            print("**Deprecation Status:** " + data[key]["deprecation_status"] + "\n", file=output_file)

    for key in keys:
        path_str = "/".join(cur_path) + "/"  + key

        if (not "value" in data[key]) and (not "alias" in data[key]):
            # This is a subsection
            print("(parameters:" + path_str + ")=", file=output_file)
            new_path = cur_path + [key]
            section_name = path_str.replace("_20", " ")
            print("## **Parameters in section** " + section_name, file=output_file)
            handle_subsection(data[key], new_path, output_file)

def handle_parameters(data):
    global_output_file = open("doc/sphinx/parameters/index.md", "w")
    global_output_file.write("(sec:parameter-documentation-home)=\n"
                             "# Parameter Documentation\n"
                             ":::{admonition} Under construction\n"
                             ":class: warning\n"
                             "\n"
                             "Migrating the ASPECT manual from LaTeX to Sphinx/MyST is not complete.\n"
                             "If what you are looking for is not here, please see the PDF version.\n"
                             ":::\n"
                             "\n"
                             ":::{toctree}\n"
                             "---\n"
                             "maxdepth: 2\n"
                             "---\n"
                             "global.md\n")
    global_parameters = open("doc/sphinx/parameters/global.md", "w")
    print("# Global \n\n", file=global_parameters)
    print("## **Parameters in section** global \n\n", file=global_parameters)

    cur_path = []
    keys = list(data.keys())
    keys.sort()
    # Since there is no end to a markdown heading besides another markdown
    # heading of equal or greater size, we need to first do parameters and
    # aliases, and then do subsections otherwise parameters get nested
    # incorrectly
    for key in keys:
        path_str = key
        true_name = key.replace("_20", " ")
        
        if "value" in data[key]:
            # this is a parameter
            print("(parameters:" + path_str + ")=", file=global_parameters)
            print("### __Parameter name:__ " + true_name, file=global_parameters)
            print("**Default value:** " +  data[key]["default_value"] + " \n", file=global_parameters)
            print("**Pattern:** " +  data[key]["pattern_description"] + " \n", file=global_parameters)
            print("**Documentation:** " + data[key]["documentation"] + " \n", file=global_parameters)
        elif "alias" in data[key]:
            # This is an alias for a parameter
            print("(parameters:" + path_str + ")=", file=global_parameters)
            aliased_name = data[key]["alias"]
            alias_path_str = ":".join(cur_path) + ":" + aliased_name.replace(" ", "_20") 
            print("### __Parameter name__: " +  true_name, file=global_parameters)
            print("**Alias:** [" + aliased_name + "](parameters:" + alias_path_str + ")\n", file=global_parameters)
            print("**Deprecation Status:** " + data[key]["deprecation_status"] + "\n", file=global_parameters)

    for key in keys:
        path_str = key
        if (not "value" in data[key]) and (not "alias" in data[key]):
            # This is a subsection
            # Add this to the toctree
            print(key + ".md", file=global_output_file)
            # Create the new file and write to it now
            subfile = open("doc/sphinx/parameters/" + key + ".md", "w")
            print("(parameters:" + path_str + ")=", file=subfile)
            new_path = cur_path + [key]
            section_name = path_str.replace("_20", " ").replace(":", "/")
            print("# **" + section_name + "**\n\n", file=subfile)
            print("## **Parameters in section** " + section_name + "\n\n", file=subfile)
            handle_subsection(data[key], new_path, subfile)
            subfile.close()
    global_output_file.write(":::\n")
    global_output_file.close()
   
if __name__  == "__main__":
    data = {}
    input_file = open(argv[1], "r")
    data = json.load(input_file)
    handle_parameters(data)
    exit(0)
