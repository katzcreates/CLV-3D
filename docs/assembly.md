# Cabinetry & Enclosure Assembly

The CLV-3D is designed around the IKEA METOD modular kitchen system, providing a sturdy and professional-looking foundation for the enclosure.

## IKEA Parts List

### Lower Section (Storage & Base)

- 2x [METOD Base cabinet frame (60x60x80 cm)](https://www.ikea.com/gb/en/p/metod-base-cabinet-frame-black-grey-60591702/)
- 2x [VALLSTENA Door (60x80 cm, White)](https://www.ikea.com/gb/en/p/vallstena-door-white-80541693/)
- [FÖRBÄTTRA Plinth (Dark grey)](https://www.ikea.com/gb/en/p/foerbaettra-plinth-dark-grey-80454110/)
- [UTRUSTA Shelf (Black-grey)](https://www.ikea.com/gb/en/p/utrusta-shelf-black-grey-10591785/)
- [HÅLLBAR Bin with lid (Light grey)](https://www.ikea.com/gb/en/p/hallbar-bin-with-lid-light-grey-20420203/)
- [UTRUSTA Hinge w b-in damper (110°)](https://www.ikea.com/gb/en/p/utrusta-hinge-w-b-in-damper-for-kitchen-80524882/)
- [UTRUSTA Hinge w b-in damper (153°)](https://www.ikea.com/gb/en/p/utrusta-hinge-w-b-in-damper-for-kitchen-10427262/)
- 4x [METOD Leg](https://www.ikea.com/gb/en/p/metod-leg-70556067/)

### Upper Section (Filtration & Housing)

- 2x [METOD Fridge/freezer top cabinet frame (60x60x40 cm)](https://www.ikea.com/gb/en/p/metod-fridge-freezer-top-cabinet-frame-black-grey-70591706/)
- [MÖCKLERYD Worktop (White laminate)](https://www.ikea.com/gb/en/p/moeckleryd-worktop-white-laminate-60582180/)

### Drawers & Storage

- [KNIVSHULT Drawer low (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-low-dark-grey-80600668/) (For Form 4, rated for weight)
- [KNIVSHULT Drawer medium (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-medium-dark-grey-30600675/) (For consumables)
- [KNIVSHULT Drawer front low (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-front-low-dark-grey-50600702/)
- [KNIVSHULT Drawer front medium (Dark grey)](https://www.ikea.com/gb/en/p/knivshult-drawer-front-medium-dark-grey-80600705/)

---

## Custom Panel Ordering

If you are using the same IKEA base furniture, you can refer to the `Cut Enclosure Pieces/` folder for DXF files for the exact pieces of acrylic and MDF that require custom shapes. For those in the UK, I recommend [CutMy.co.uk](https://cutmy.co.uk/).

### 3mm Clear Acrylic

| Dimensions (mm) | Qty | Shape | Note |
| :--- | :--- | :--- | :--- |
| 1200 x 297 | 1 | Rectangle | Top Door |
| 664 x 793 | 2 | Custom (Upload) | Side Panels (`AcrylicSide_FinalFix.dxf`) |
| 600 x 495 | 2 | Rectangle | Side Doors |

### 3mm Medite Premier MDF

| Dimensions (mm) | Qty | Shape | Note |
| :--- | :--- | :--- | :--- |
| 1194 x 797 | 1 | Custom (Upload) | Backsplash (`Backsplash_FinalFix.dxf`) |
| 329 x 285 | 1 | Custom (Upload) | Filter Baffle (`FilterBlockerFix.dxf`) |

---

## Enclosure Assembly Steps

1. **Upper/Wall Cabinets**: Install wall cabinets and secure to wall.
2. **Base Cabinets**: Assemble METOD base cabinets and test placement.
3. **Internal Prep**: Cut airflow holes in the back of the lower cabinet and countertop.
4. **Plenum Construction**: Create the rear plenum using foam/MDF.
5. **Base Placement:** Verify leg height is level and place base cabinet so plenum edges are flush against the wall.
6. **Drawers, Countertop & Hardware**: Install drawers and countertop and verify printer clearance.
7. **Backsplash:** Add in backsplash and secure to cabinets/countertop. Seal any major gaps or holes.
8. **Ventilation Install**: Mount inline fan and carbon filter to the bottom of the upper cabinets.
9. **Panels and Filters**: Install acrylic side panels, secure with L-brackets, and mount MDF baffle.
10. **Doors and Hinges:** Align hinges and mount the top and side doors.
11. **Electronics**: Integrate the ESP32-S3 nodes and sensor suite.

> [!TIP]
> **Gravity & Alignment**
> Real-world assembly often differs from CAD models due to mounting tolerances and "cabinet sag." It is highly recommended to complete the physical frame assembly and measure the specific "as-built" openings before cutting your final acrylic panels.

---

*This project is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).*
