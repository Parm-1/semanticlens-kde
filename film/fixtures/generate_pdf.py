#!/usr/bin/env python3
"""Recreate the authored two-page PDF fixture; requires reportlab."""
from pathlib import Path
from reportlab.pdfgen import canvas
p=Path(__file__).resolve().parent/'v1/parcel-pdf/parcel.pdf'
p.parent.mkdir(parents=True,exist_ok=True)
c=canvas.Canvas(str(p),pagesize=(400,500),invariant=1,pageCompression=0)
c.setTitle('Parcel — authored screenplay fixture')
for heading,line in [('INT. PARCEL ROOM - MORNING','ADA: Every parcel has a number.'),('EXT. QUIET PLATFORM - EVENING','ADA: PARC-20942 belongs to the last passenger.')]:
 c.setFont('Helvetica',12);c.drawString(24,470,heading);c.drawString(24,435,line);c.showPage()
c.save()
