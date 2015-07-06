#! /usr/bin/env python

import sys
import os
import commands


def eus2urdf_for_gazebo_pyscript (name, collada_path, overwrite=True):
    urdf_dir_path = commands.getoutput('rospack find eusurdf') + '/models/' + name
    if overwrite:
        os.system("rm -rf %s" % urdf_dir_path)
    else:
        print "[eus2urdf] check if the same name model already exits"
        if os.path.exists(urdf_dir_path):
            print '[ERROR eus2urdf] the same name model already exits'
            exit(1)

    print "[eus2urdf] make directory for urdf"
    os.mkdir(urdf_dir_path)

    add_line_string = '\<uri\>file://%s\</uri\>' % name
    manifest_path = '%s/../manifest.xml' % urdf_dir_path
    if len(commands.getoutput("grep %s %s" % (add_line_string, manifest_path))) == 0:
        print "[eus2urdf] add file path to manifest.xml"
        os.system('sed -i -e \"s@  </models>@    %s\\n  </models>@g\" %s' % (add_line_string, manifest_path))

    print "[eus2urdf] make model.config in urdf directory"
    config_path = '%s/model.config' % urdf_dir_path
    os.system('echo "<?xml version=\'1.0\'?>\n<model>\n  <name>%s</name>\n  <version>0.1.0</version>\n  <sdf>model.urdf</sdf>\n  <description>\n     This model was automatically generated by converting the eus model.\n  </description>\n</model>\n" > %s' % (name, config_path))

    print "[eus2urdf] convert collada to urdf"
    meshes_path = urdf_dir_path + '/meshes'
    urdf_path = urdf_dir_path + '/' + 'model.urdf'
    os.mkdir(meshes_path)
    os.system('rosrun collada_urdf_jsk_patch collada_to_urdf %s -G -A --mesh_output_dir %s --mesh_prefix "model://%s/meshes" -O %s' % (collada_path, meshes_path, name, urdf_path))
    os.system('sed -i -e "s@continuous@revolute@g" %s' % urdf_path)
    os.system('sed -i -e \"s@<robot name=\\"inst_kinsystem\\"@<robot name=\\"%s\\"@g\" %s' % (name, urdf_path))
    os.system('sed -i -e \"1,/  <link /s/  <link /  <gazebo>\\n    <static>false<\/static>\\n  <\/gazebo>\\n  <link /\" %s' % urdf_path)

    # print "[eus2urdf] add inertia property to urdf   # Inertia value is not correct. Inertia should be added at eus2collada."
    os.system('sed -i -e \"s@      <inertia ixx=\\\"1e-09\\\" ixy=\\\"0\\\" ixz=\\\"0\\\" iyy=\\\"1e-09\\\" iyz=\\\"0\\\" izz=\\\"1e-09\\\"/>@      <inertia ixx=\\\"1e-03\\\" ixy=\\\"0\\\" ixz=\\\"0\\\" iyy=\\\"1e-03\\\" iyz=\\\"0\\\" izz=\\\"1e-03\\\"/>@g\" %s' % urdf_path)
    # os.system('sed -i -e \"s@  </link>@    <inertial>\\n      <mass value=\\\"20\\\"/>\\n      <origin xyz=\\\"0 0 0.2\\\" rpy=\\\"0 0 0\\\"/>\\n      <inertia ixx=\\\"1\\\" ixy=\\\"0\\\" ixz=\\\"0\\\" iyy=\\\"1\\\" iyz=\\\"0\\\" izz=\\\"0\\\"/>\\n    </inertial>\\n  </link>@g\" %s' % urdf_path)


if __name__ == '__main__':
    if len(sys.argv) > 2:
        eus2urdf_for_gazebo_pyscript(sys.argv[1], sys.argv[2])
