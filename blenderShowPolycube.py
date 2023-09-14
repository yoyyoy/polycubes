bl_info = {
    "name" : "Show Polycube",
    "blender" : (3,6,0),
    "category" : "Object",
}

import bpy

class ShowPolycube(bpy.types.Operator):
    bl_idname="object.show_polycube"
    bl_label = "Show Polycube"
    bl_options = {'REGISTER', 'UNDO'}
    
    #blender doesn't like ints>2^31 so split index into 2
    billionsIndex: bpy.props.IntProperty(name="Billions", default=0, min=0, max=7)
    millionsIndex: bpy.props.IntProperty(name="Millions", default=0, min=0, max=999999999)
    def execute(self, context):
        scene = context.scene
        
        #CHANGE THIS PATH TO WHERE YOUR ACTUAL FILE IS
        polycubeFile = open("D:/polycube/output15.polycubes", "rb")
        count=0
        polycubeIndex=(self.billionsIndex*1000000000)+self.millionsIndex
        #a linear search through the file because I can't think of anything better
        #makes viewing shapes that occur billions of shapes in the file impractical
        while count < polycubeIndex:
            xlength=int.from_bytes(polycubeFile.read(1),"big")
            ylength=int.from_bytes(polycubeFile.read(1),"big")
            zlength=int.from_bytes(polycubeFile.read(1),"big")
            byteCount=xlength*ylength*zlength
            byteCount-=1
            byteCount//=8
            byteCount+=1
            polycubeFile.read(byteCount)
            count+=1
            
        x=int.from_bytes(polycubeFile.read(1),"big")
        y=int.from_bytes(polycubeFile.read(1),"big")
        z=int.from_bytes(polycubeFile.read(1),"big")
        shapeBytes=x*y*z
        shapeBytes-=1
        shapeBytes//=8
        shapeBytes+=1
        shapeData=polycubeFile.read(shapeBytes)
        for zPos in range(z):
            for yPos in range(y):
                for xPos in range(x):
                    bitPos = (zPos*y*x)+(yPos*x)+xPos
                    bitMod=bitPos%8
                    bitPos//=8
                    if (shapeData[bitPos] & (1 << bitMod)) > 0:
                        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(xPos+0.5, yPos+0.5, zPos+0.5))
        
        return {'FINISHED'}

def menu_func(self, context):
    self.layout.operator(ShowPolycube.bl_idname)

def register():
    bpy.utils.register_class(ShowPolycube)
    bpy.types.VIEW3D_MT_object.append(menu_func)
    
def unregister():
    bpy.utils.unregister_class(ShowPolycube)
    bpy.types.VIEW3D_MT_object.remove(menu_func)
    
if __name__ == "__main__":
    register()