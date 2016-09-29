# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Header, Menu, Panel


class INFO_HT_header(Header):
    bl_space_type = 'UVS_EDITOR'

    def draw(self, context):
        layout = self.layout

        window = context.window
        scene = context.scene
        rd = scene.render

        row = layout.row(align=True)
        row.template_header()

        row.operator("uvs.add_images")


class UVS_PT_settings(Panel):
    bl_space_type = 'UVS_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Settings"

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        col = layout.column()
        col.prop(sc, "min_span_u")
        col.prop(sc, "max_span_u")
        col.prop(sc, "min_span_v")
        col.prop(sc, "max_span_v")
        col.prop(sc, "show_udim_numbers")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
