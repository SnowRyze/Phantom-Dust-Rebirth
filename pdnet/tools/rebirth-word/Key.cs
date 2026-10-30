using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

// Lifts lettering off a painted 8 px grey checkerboard: the checker behind each pixel is known (one 16x16 tile
// averaged from a clean corner), so colour = a*F + (1-a)*B gives F once a is chosen from how far the pixel is from B.
public static class Key
{
    public static double[] Tile(Bitmap s, int x0, int y0, int x1, int y1)
    {
        var sum = new double[256]; var n = new int[256];
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) { int i = (y & 15) * 16 + (x & 15); sum[i] += s.GetPixel(x, y).R; n[i]++; }
        for (int i = 0; i < 256; ++i) sum[i] /= Math.Max(1, n[i]);
        return sum;
    }

    static double Smooth(double e0, double e1, double v) { double t = Math.Max(0, Math.Min(1, (v - e0) / (e1 - e0))); return t * t * (3 - 2 * t); }

    // Straight-alpha float RGBA, w*h*4.
    public static float[] Lift(Bitmap s, double[] tile, out int w, out int h)
    {
        w = s.Width; h = s.Height;
        var o = new float[w * h * 4];
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                Color c = s.GetPixel(x, y);
                double bg = tile[(y & 15) * 16 + (x & 15)];
                double lum = (c.R + c.G + c.B) / 3.0, chroma = Math.Max(c.R, Math.Max(c.G, c.B)) - Math.Min(c.R, Math.Min(c.G, c.B));
                double d = lum - bg;
                double a = Math.Max(Smooth(5, 16, chroma), Math.Max(Smooth(10, 26, d), Smooth(8, 20, -d) * Math.Min(1, -d / bg * 1.15)));
                if (a < 0.02) continue;
                a = Math.Min(1, a);
                int i = (y * w + x) * 4;
                double[] p = { c.R, c.G, c.B };
                for (int k = 0; k < 3; ++k) o[i + k] = (float)Math.Max(0, Math.Min(255, (p[k] - (1 - a) * bg) / a));
                o[i + 3] = (float)a;
            }
        return o;
    }

    public static Rectangle Bounds(float[] o, int w, int h, float min)
    {
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) if (o[(y * w + x) * 4 + 3] > min) { x0 = Math.Min(x0, x); x1 = Math.Max(x1, x); y0 = Math.Min(y0, y); y1 = Math.Max(y1, y); }
        return Rectangle.FromLTRB(x0, y0, x1 + 1, y1 + 1);
    }

    // Area-average shrink of rectangle r to height th (premultiplied), as a 32bpp bitmap.
    public static Bitmap Shrink(float[] o, int w, Rectangle r, int th)
    {
        double f = th / (double)r.Height;
        int tw = (int)Math.Round(r.Width * f);
        var b = new Bitmap(tw, th, PixelFormat.Format32bppArgb);
        for (int ty = 0; ty < th; ++ty)
            for (int tx = 0; tx < tw; ++tx)
            {
                double sx0 = tx / f, sx1 = (tx + 1) / f, sy0 = ty / f, sy1 = (ty + 1) / f, area = 0;
                double[] sum = new double[4];
                for (int sy = (int)sy0; sy < sy1 && sy < r.Height; ++sy)
                    for (int sx = (int)sx0; sx < sx1 && sx < r.Width; ++sx)
                    {
                        double cv = (Math.Min(sx1, sx + 1) - Math.Max(sx0, sx)) * (Math.Min(sy1, sy + 1) - Math.Max(sy0, sy));
                        int i = ((r.Y + sy) * w + r.X + sx) * 4;
                        double a = o[i + 3];
                        for (int k = 0; k < 3; ++k) sum[k] += o[i + k] * a * cv;
                        sum[3] += a * cv; area += cv;
                    }
                if (sum[3] <= 0) { b.SetPixel(tx, ty, Color.Transparent); continue; }
                int A = (int)Math.Round(Math.Min(255, sum[3] / area * 255));
                Func<int, int> ch = k => (int)Math.Round(Math.Min(255, sum[k] / sum[3]));
                b.SetPixel(tx, ty, Color.FromArgb(A, ch(0), ch(1), ch(2)));
            }
        return b;
    }

    public static void Header(string path, Bitmap word)
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("#pragma once");
        sb.AppendLine("// \"Rebirth\" for the title screen (gamepatches.cpp, RebirthRows): straight-alpha BGRA, made by pdnet\\tools\\rebirth-word\\make.ps1 from the");
        sb.AppendLine("// supplied lettering artwork (rebirth-art.png, cropped to what is visible). Do not edit.");
        sb.AppendLine("#include <cstdint>");
        sb.AppendFormat("constexpr int kRebirthWordW = {0}, kRebirthWordH = {1};\n", word.Width, word.Height);
        sb.Append("constexpr uint8_t kRebirthWord[] = {");
        int n = 0;
        for (int y = 0; y < word.Height; ++y)
            for (int x = 0; x < word.Width; ++x)
            {
                Color c = word.GetPixel(x, y);
                if (n++ % 8 == 0) sb.Append("\n    ");
                sb.AppendFormat("{0},{1},{2},{3}, ", c.B, c.G, c.R, c.A);
            }
        sb.AppendLine("\n};");
        File.WriteAllText(path, sb.ToString());
    }

    // The keyed word over a flat colour, zoomed, to check the cut.
    public static void Preview(string path, Bitmap word, Color bg, int zoom)
    {
        var big = new Bitmap(word.Width * zoom + 20, word.Height * zoom + 20);
        using (var g = Graphics.FromImage(big))
        {
            g.Clear(bg);
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
            g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
            g.DrawImage(word, 10, 10, word.Width * zoom, word.Height * zoom);
        }
        big.Save(path, ImageFormat.Png);
    }

    public static Bitmap Full(float[] o, int w, Rectangle r)
    {
        var b = new Bitmap(r.Width, r.Height, PixelFormat.Format32bppArgb);
        for (int y = 0; y < r.Height; ++y)
            for (int x = 0; x < r.Width; ++x)
            {
                int i = ((r.Y + y) * w + r.X + x) * 4;
                b.SetPixel(x, y, Color.FromArgb((int)(o[i + 3] * 255), (int)o[i], (int)o[i + 1], (int)o[i + 2]));
            }
        return b;
    }
}
