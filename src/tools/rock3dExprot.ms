/*
ÍÅËÜÇß ÈÑÏÎËÜÇÎÂÀÒÜ ÎÒÐÈÖÀÒÅËÜÍÎÅ ÌÀÑØÒÀÁÈÐÎÂÀÍÈÅ, ÒÀÊ ÊÀÊ ÐÅÀËÈÇÀÖÈÈ
Â 3Ä ÌÀÊÑÅ È DIRECTX ÐÀÇËÈ×ÀÞÒÑß (3Ä ÌÀÊÑ ÀÂÒÎÌÀÒÈ×ÅÑÊÈ ÌÅÍßÅÒ
ÏÎÐßÄÎÊ ÎÁÕÎÄÀ ÂÅÐØÈÍ)
*/

global curExportVersion = 0
global curExpMap = 1

rollout ExportMesh "Export Mesh"
(
    EditText editPath "Path" pos:[10, 10] width:210
	Button btnBrowse "Browse" pos:[227, 8] width:50
	CheckBox chkTextureCoord "Saving of texture coordinates" checked:true
	CheckBox chkConvToLCS "Convert from right to left coordinate system" checked:false

	Button btnExport "Export" width:270

	on ExportMesh open do
	(
	    editPath.Text = "c:\\*.r3d"
	)

	on btnBrowse pressed do
	(
	    s = GetSaveFileName types:"Rock3d file (*.r3d)|*.r3d"
		if s != undefined then
		    editPath.Text = s
	)

	on btnExport pressed do
	(
	    if editPath.Text == undefined then
		(
		    MessageBox "Îøèáêà, ââåäèòå êîððåêòíûé ïóòü"
			exit
		)
		for i = 1 to selection.count do
		(
			filePath = editPath.Text;
			if selection.count > 1 then
			    filePath = (getFilenamePath filePath) + (getFilenameFile filePath) + (i as string) + ".r3d"

			fs = fOpen filePath "wb";
            if fs == undefined then
			(
			    MessageBox "Îøèáêà çàïèñè â ôàéë, ïðîöåññ áóäåò ïðåêðàùåí..."
				exit
			)

			curMesh = ConvertToMesh selection[i]

            saveExpMap = chkTextureCoord.Checked and meshop.getMapSupport curMesh curExpMap
			bitSaveExpMap = 0
		    if saveExpMap then
                bitSaveExpMap = 1
			convToLCS = chkConvToLCS.Checked
			bitConvToLCS = 0
			if convToLCS then
			    bitConvToLCS = 1

			meshop.deleteIsoMapVertsAll curMesh
			meshop.deleteIsoVerts curMesh

			numUniqueVerts = meshop.GetNumVerts curMesh
			numFaces = meshop.GetNumFaces curMesh

			--association container of Materials ID
			mathIdKeys = #()    --[key]=MathID
			mathIdValues = #()  --[key][numFace]=Index vertex

			--faces using this vertex
            facesUsingUVerts = #()  --[IndexVert][numRefrence]=[numFace, numVertFace, numIndexedVertex]
            facesUsingUVerts.Count = numUniqueVerts

			--index buffer
			indexBuf = #()
			indexBuf.Count = numFaces * 3

			WriteLong fs curExportVersion
			WriteByte fs bitConvToLCS
		    WriteByte fs bitSaveExpMap
			headerSize = ftell fs
			numVertexs = 0
			WriteLong fs numVertexs

            --Matherial ID buffer
			for i = 1 to numFaces do
			(
			    curFaceMatID = GetFaceMatID curMesh i
				mathIndex = findItem mathIdKeys curFaceMatID
                if mathIndex > 0 then
				    append mathIdValues[mathIndex] i
				else
				(
				    append mathIdKeys curFaceMatID
					append mathIdValues #(i)
                )
			)

			curIndexedVert = 0;
			for mathIndex = 1 to mathIdValues.Count do
			for faceIndex = 1 to mathIdValues[mathIndex].Count do
			(
			    i = mathIdValues[mathIndex][faceIndex]
			    curFace = GetFace curMesh i
				curFaceSmoothGr = GetFaceSmoothGroup curMesh i
				local curMapFace
				if saveExpMap then
					curMapFace = meshop.GetMapFace curMesh curExpMap i
				for j = 1 to 3 do
			    (
                    curIndexedVert = curIndexedVert + 1;
				    curIndVert = curFace[j]
  					local curIndMapVert
					if saveExpMap then
						curIndMapVert = curMapFace[j]

				    findEqualVertex = false
				    if facesUsingUVerts[curIndVert] == undefined then
				        facesUsingUVerts[curIndVert] = #([i, j, curIndexedVert]) --add first refrence
				    else
				    (
				        for k = 1 to facesUsingUVerts[curIndVert].Count do
					    (
					        nxI = facesUsingUVerts[curIndVert][k][1]
                            nxJ = facesUsingUVerts[curIndVert][k][2]
                            nextIndexedVert = facesUsingUVerts[curIndVert][k][3]
						    nextFaceSmoothGr = GetFaceSmoothGroup curMesh nxI
						    local nextMapFace
							local nextIndMapVert
							if saveExpMap then
							(
								nextMapFace = meshop.GetMapFace curMesh curExpMap nxI
								nextIndMapVert = nextMapFace[nxJ]
							)

						    if curIndexedVert == nextIndexedVert then
						        MessageBox("Âûðîæäåííûé ôåéñ! Íåïðåäóñìîòðåííûé ñëó÷àé...")

						    --Criteries unique
						    if nextFaceSmoothGr > 0 and nextFaceSmoothGr == curFaceSmoothGr and
                               (saveExpMap == false or nextIndMapVert == curIndMapVert) then
						    (
						        indexBuf[curIndexedVert] = indexBuf[nextIndexedVert]
                                findEqualVertex = true
                                exit
						    )
					    )
					    append facesUsingUVerts[curIndVert] [i, j, curIndexedVert] --add next refrence
				    )
				    if findEqualVertex == false then
				    (
				        indexBuf[curIndexedVert] = numVertexs

					    --saving vertex
					    curVert = meshop.GetVert curMesh curIndVert
					    if convToLCS then
					        swap curVert.x curVert.y
					    WriteFloat fs curVert.x
					    WriteFloat fs curVert.y
					    WriteFloat fs curVert.z

					    --saving normals
					    curFaceNormals = meshop.GetFaceRNormals curMesh i
					    local curNormal
					    if curFaceSmoothGr == 0 or j > curFaceNormals.Count then
					        curNormal = GetFaceNormal curMesh i
					    else
					        curNormal =	curFaceNormals[j]
					    if convToLCS then
					        swap curNormal.x curNormal.y
					    WriteFloat fs curNormal.x
					    WriteFloat fs curNormal.y
					    WriteFloat fs curNormal.z

					    --saving current map channal
					    if saveExpMap == true then
					    (
						    curMapVert = meshop.GetMapVert curMesh curExpMap curIndMapVert
						    WriteFloat fs curMapVert.x
						    WriteFloat fs (1 - curMapVert.y)
					    )

                        numVertexs = numVertexs + 1
				    )
			    )
			)

			--rewrite count vertex
			fseek fs headerSize #seek_set
			WriteLong fs numVertexs
			fseek fs 0 #seek_end

			--saving index buffer
			WriteLong fs numFaces
			for i = 1 to numFaces * 3 do
			    WriteLong fs indexBuf[i]

		    --saving MathId
			faceStart = 0
			WriteLong fs mathIdKeys.Count --Materials count
			for i = 1 to mathIdKeys.Count do
			(
			    WriteLong fs mathIdKeys[i] --MathID
			    WriteLong fs faceStart
			    WriteLong fs mathIdValues[i].Count
				faceStart = faceStart + mathIdValues[i].Count;
			)

			fClose fs
		)
	)
)

m_roll = NewRolloutFloater "Export to r3d" 300 135
AddRollout ExportMesh m_roll