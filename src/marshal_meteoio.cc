#include "marshal_meteoio.h"




void marshal_CSpecialPTSArray(paroc_buffer &buf,CSpecialPTSArray &data, int maxsize, int flag, paroc_memspool *temp)
{
  if (flag & FLAG_MARSHAL)
    {
      int n=data.GetSize();
      buf.Pack(&n,1);
      if (n) buf.Pack((int *)((SPECIAL_PTS *)&data[0]), 2*n);
    }
  else
    {
      int n;

      buf.UnPack(&n,1);
      data.SetSize(n);
      if (n) buf.UnPack((int *)((SPECIAL_PTS *)&data[0]), 2*n);
    }
}

void marshal_vector_MeteoData(paroc_buffer &buf, vector<MeteoData> &data, int maxsize, int flag, paroc_memspool *temp){
  if(flag&FLAG_MARSHAL)
  {
    int n=data.size();
    buf.Pack(&n,1);
    for(int i=0;i<n;i++)
    {
      data[i].Serialize(buf,true);
    }
  }
  else
  {
    int n=0;
    buf.UnPack(&n,1);
    data.clear();  
    for(int i=0;i<n;i++)
    {
      MeteoData obj;
      obj.Serialize(buf,false);
      data.push_back(obj);
    }
  }
}


void marshal_vector_StationData(paroc_buffer &buf, vector<StationData> &data, int maxsize, int flag, paroc_memspool *temp){
  if(flag&FLAG_MARSHAL)
  {
    int n=data.size();
    buf.Pack(&n,1);
    for(int i=0;i<n;i++)
    {
      data[i].Serialize(buf,true);
    }
  }
  else
  {
    int n=0;
    buf.UnPack(&n,1);
    data.clear();
    for(int i=0;i<n;i++)
    {
      StationData obj;
      obj.Serialize(buf,false);
      data.push_back(obj);
    }
  }
}

void marshal_vector_Grid2DObject(paroc_buffer &buf, vector<Grid2DObject> &data, int maxsize, int flag, paroc_memspool *temp){
  assert(false); /* This line is here to check if the method is used*/
  if(flag&FLAG_MARSHAL)
  {
    int n=data.size();
    buf.Pack(&n,1);
    for(int i=0;i<n;i++)
    {
      data[i].Serialize(buf,true);
    }
  }
  else
  {
    int n=0;
    buf.UnPack(&n,1);
    data.clear();
    for(int i=0;i<n;i++)
    {
      Grid2DObject obj; 
      //buf.UnPack(&obj,1);
      obj.Serialize(buf,false);
      //marshal_Grid2DObject(buf, *obj, 0, flag, NULL);
      data.push_back(obj);
    }
  }
}

void marshal_TYPE_DOUBLE2D(paroc_buffer &buf, TYPE_DOUBLE2D &data,int maxsize, int flag, paroc_memspool *temp)
{
  if (flag & FLAG_MARSHAL)
    {
      //int dim[2];
      int nx,ny;
      data.GetSize(nx,ny);

      buf.Pack(&nx,1);
      buf.Pack(&ny,1);
      //double **tmp=(double**)&data[0];//double **tmp=data;
    
      if (nx>0 && ny>0)
	{
          //for (int i=0;i<nx;i++,tmp++) buf.Pack(*tmp,ny);
          for (int i=0;i<nx;i++) buf.Pack(&data[i][0],ny);
	}
    }
  else
    {
    
      int nx,ny;//dim[2];
      buf.UnPack(&nx,1);
      buf.UnPack(&ny,1);
      data.Create(nx,ny);
      //double **tmp=data;
    
      if (nx>0 && ny>0)
	{
	  //for (int i=0;i<nx;i++,tmp++) buf.UnPack(*tmp,ny); 
	  for (int i=0;i<nx;i++) buf.UnPack(&data[i][0],ny); 
	}
    }
}

void marshal_CDoubleArray(paroc_buffer &buf, CDoubleArray &data,int maxsize, int flag, paroc_memspool *temp)
{
  assert(false); /* This line is here to check if the method is used*/
  if (flag & FLAG_MARSHAL)
    {
      int n=data.GetSize();
      buf.Pack(&n,1);
      if (n) buf.Pack((double *)&data[0],n);
    }
  else
    {
      int n;
      buf.UnPack(&n,1);
      data.SetSize(n);
      if (n) buf.UnPack((double *)&data[0],n);
    }
       
}

void marshal_CNodeArray(paroc_buffer &buf,CNodeArray &data,int maxsize, int flag, paroc_memspool *temp)
{
  assert(false); /* This line is here to check if the method is used*/
  if (flag & FLAG_MARSHAL)
    {
      int n=data.GetSize();
      buf.Pack(&n,1);
      if (n)
	{
	  buf.Pack((double *)((NODE *)&data[0]),n*(sizeof(NODE)/sizeof(double)));
	}
    }
  else
    {
      int n;
      buf.UnPack(&n,1);
      data.SetSize(n);
      if (n)
	{
	  buf.UnPack((double *)((NODE *)&data[0]),n*(sizeof(NODE)/sizeof(double)));
	}
    }
}

void marshal_update_CNodeArray(paroc_buffer &buf,CNodeArray &data,int maxsize, int flag, paroc_memspool *temp)
{
  assert(false); /* This line is here to check if the method is used*/
  if (flag & FLAG_MARSHAL)
    {
      int n=data.GetSize();
      buf.Pack(&n,1);
      NODE *tmp=&data[0];
      for (int i=0;i<n;i++,tmp++)
	{
	  buf.Pack(&(tmp->u),1);
	  buf.Pack(&(tmp->v),1);
	  buf.Pack(&(tmp->w),1);
	  buf.Pack(&(tmp->slope),1);
	  buf.Pack(&(tmp->sl),1);

	  buf.Pack(&(tmp->tet),1);
	  buf.Pack(&(tmp->p),1);
	  buf.Pack(&(tmp->Km),1);
	  buf.Pack(&(tmp->lm),1);

	  buf.Pack(&(tmp->wstar),1);
	  buf.Pack(&(tmp->e),1);
	}
    }
  else
    {
      int n;
      buf.UnPack(&n,1);
      data.SetSize(n);
      NODE *tmp=&data[0];
      for (int i=0;i<n;i++,tmp++)
	{
	  buf.UnPack(&(tmp->u),1);
	  buf.UnPack(&(tmp->v),1);
	  buf.UnPack(&(tmp->w),1);
	  buf.UnPack(&(tmp->slope),1);
	  buf.UnPack(&(tmp->sl),1);

	  buf.UnPack(&(tmp->tet),1);
	  buf.UnPack(&(tmp->p),1);
	  buf.UnPack(&(tmp->Km),1);
	  buf.UnPack(&(tmp->lm),1);

	  buf.UnPack(&(tmp->wstar),1);
	  buf.UnPack(&(tmp->e),1);
	}
    }
}

void marshal_input_CNodeArray(paroc_buffer &buf,CNodeArray &data,int maxsize, int flag, paroc_memspool *temp)
{
  assert(false); /* This line is here to check if the method is used*/
  if (flag & FLAG_MARSHAL)
    {
      int n=data.GetSize();
      buf.Pack(&n,1);
      NODE *tmp=&data[0];
      for (int i=0;i<n;i++,tmp++)
	{
	  buf.Pack(&(tmp->u),1);
	  buf.Pack(&(tmp->v),1);
	  buf.Pack(&(tmp->w),1);
	  buf.Pack(&(tmp->tet),1);
	  buf.Pack(&(tmp->p),1);
	  buf.Pack(&(tmp->Km),1);
	  buf.Pack(&(tmp->lm),1);
	  buf.Pack(&(tmp->e),1);
	}
    }
  else
    {
      int n;
      buf.UnPack(&n,1);
      data.SetSize(n);
      NODE *tmp=&data[0];
      for (int i=0;i<n;i++,tmp++)
	{
	  buf.UnPack(&(tmp->u),1);
	  buf.UnPack(&(tmp->v),1);
	  buf.UnPack(&(tmp->w),1);
	  buf.UnPack(&(tmp->tet),1);
	  buf.UnPack(&(tmp->p),1);
	  buf.UnPack(&(tmp->Km),1);
	  buf.UnPack(&(tmp->lm),1);
	  buf.UnPack(&(tmp->e),1);
	}
    }
}

