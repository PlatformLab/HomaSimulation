
int DrawRectangle(int x, int y, int width, int height, int transient);
int FillRectangle(int x, int y, int width, int height, int transient);
int DrawPolygon(XPoint *pts, int no_of_pts, int transient);
int FillPolygon(XPoint *pts, int no_of_pts, int transient);
int DrawOval(int x, int y, int xr, int yr, int transient);
int DrawLine(int x, int y, int xr, int yr, int transient);
int DrawString(char *font, char *str, int x, int y, int transient);
int ClearDisplay();
int UpdateDisplay();
