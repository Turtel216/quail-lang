import os
import sys
import lit.formats
import lit.util

# Suite name and format
config.name = 'Quail'
config.test_format = lit.formats.ShTest(execute_external=True)

# Test suffixes
config.suffixes = ['.ql']

# Test root paths
config.test_source_root = os.path.dirname(__file__)

if hasattr(config, 'quail_obj_root') and config.quail_obj_root:
    config.test_exec_root = os.path.join(config.quail_obj_root, 'test')
else:
    config.test_exec_root = config.test_source_root

# Ensure prelude and runtime symlinks exist in test_exec_root and subdirectories
for res_folder in ['prelude', 'runtime']:
    res_src = os.path.join(config.quail_src_root, res_folder)
    if os.path.exists(res_src):
        for root, dirs, _ in os.walk(config.test_source_root):
            rel_path = os.path.relpath(root, config.test_source_root)
            target_dir = os.path.join(config.test_exec_root, rel_path) if rel_path != '.' else config.test_exec_root
            os.makedirs(target_dir, exist_ok=True)
            link_dst = os.path.join(target_dir, res_folder)
            if not os.path.exists(link_dst):
                try:
                    os.symlink(res_src, link_dst)
                except OSError:
                    pass

# Utility to locate tools
def find_tool(name, site_path, default_name=None):
    if site_path and os.path.exists(site_path):
        return site_path
    if default_name is None:
        default_name = name
    return lit.util.which(default_name, config.environment.get('PATH', '')) or default_name

qc_bin = find_tool('qc', getattr(config, 'qc_executable', None), 'qc')
filecheck_bin = find_tool('FileCheck', getattr(config, 'filecheck_executable', None), 'FileCheck')

run_qc_script = os.path.join(config.quail_src_root, 'test', 'run_qc.py')

# Substitutions
qc_cmd = f"{sys.executable} {run_qc_script} {qc_bin}"
config.substitutions.append(('%qc', qc_cmd))
config.substitutions.append(('%FileCheck', filecheck_bin))

# Ensure PATH includes LLVM tools directory if provided
if hasattr(config, 'llvm_tools_dir') and config.llvm_tools_dir:
    config.environment['PATH'] = os.path.pathsep.join([
        config.llvm_tools_dir,
        config.environment.get('PATH', '')
    ])
